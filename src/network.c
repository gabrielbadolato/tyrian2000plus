/*
 * OpenTyrian: A modern cross-platform port of Tyrian
 * Copyright (C) The OpenTyrian Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include "network.h"

#include "episodes.h"
#include "fonthand.h"
#include "joystick.h"
#include "keyboard.h"
#include "lang.h"
#include "logging.h"
#include "nortsong.h"
#include "opentyr.h"
#include "palette.h"
#include "picload.h"
#include "sprite.h"
#include "varz.h"
#include "video.h"

#include <assert.h>
#include <setjmp.h>
#include <string.h>

/*                              HERE BE DRAGONS!
 *
 * When I wrote this code I thought it was wonderful... that thought was very
 * wrong.  It works, but good luck understanding how... I don't anymore.
 *
 * Hopefully it'll be rewritten some day.
 */

// increment whenever networking changes might create incompatibility; Tyrian 2000+
// has its own numbers (its games differ from OpenTyrian2000's, and the connect
// packet carries the game's version too), so the two refuse to play together
#define NET_VERSION       0x2001
#define NET_PORT          1333         // UDP

#define NET_PACKET_SIZE   256
#define NET_STATE_SIZE    36           // state packets: 28 as in OpenTyrian, then the state checksum

#define NET_RETRY         640          // ticks to wait for packet acknowledgment before resending
#define NET_RESEND        320          // ticks to wait before requesting unreceived game packet
#define NET_KEEP_ALIVE    1600         // ticks to wait between keep-alive packets
#define NET_TIME_OUT      16000        // ticks to wait before considering connection dead

bool isNetworkGame = false;
int network_delay = 1 + 1;  // minimum is 1 + 0

char *network_opponent_host = NULL;

Uint16 network_player_port = NET_PORT,
       network_opponent_port = NET_PORT;

static char empty_string[] = "";
char *network_player_name = empty_string,
     *network_opponent_name = empty_string;

#ifdef WITH_NETWORK
static UDPsocket socket;
static IPaddress ip;

// Tyrian 2000+: the other player is a second copy of the game on this computer
bool network_peer_local = false;

// Tyrian 2000+: 127.x.x.x or one of this computer's own addresses
static bool address_is_local(const IPaddress *address)
{
	if ((SDLNet_Read32(&address->host) >> 24) == 127)
		return true;

	IPaddress addresses[16];
	const int n = SDLNet_GetLocalAddresses(addresses, (int)COUNTOF(addresses));
	for (int i = 0; i < n; i++)
		if (addresses[i].host == address->host)
			return true;
	return false;
}

UDPpacket *packet_out_temp;
static UDPpacket *packet_temp;

UDPpacket *packet_in[NET_PACKET_QUEUE] = { NULL },
          *packet_out[NET_PACKET_QUEUE] = { NULL };

static Uint16 last_out_sync = 0, queue_in_sync = 0, queue_out_sync = 0, last_ack_sync = 0;
static Uint32 last_in_tick = 0, last_out_tick = 0;

UDPpacket *packet_state_in[NET_PACKET_QUEUE] = { NULL };
static UDPpacket *packet_state_in_xor[NET_PACKET_QUEUE] = { NULL };
UDPpacket *packet_state_out[NET_PACKET_QUEUE] = { NULL };

static Uint16 last_state_in_sync = 0, last_state_out_sync = 0;
static Uint32 last_state_in_tick = 0;

static bool net_initialized = false;
static bool connected = false, quit = false;

// Tyrian 2000+: the host (player 1 from the Network Game menu) learns the
// other player's address from the first connect packet
static bool waiting_for_opponent = false;

// Tyrian 2000+: the round trips measured with PACKET_PING, in ms
#define NET_PINGS 12
static Uint32 ping_ms[NET_PINGS];
static int pings_received = 0;

// Tyrian 2000+: a checksum of each frame's game state, by state sync number;
// every NET_CHECK_INTERVAL-th state packet carries one, so that games that
// went different ways stop instead of going on unseen
#define NET_CHECK_INTERVAL 35
#define NET_CHECK_RING     64
static Uint32 frame_checksum[NET_CHECK_RING];
#endif

bool network_delay_fixed = false;  // --net-delay given: do not measure
bool network_from_menu = false;    // started from the Network Game menu (not --net)
char network_opponent_version[32] = "";

// where the game goes back to after a network game ends (opentyr.c); NULL: the program closes
jmp_buf *network_abort = NULL;

uint thisPlayerNum = 0;  /* Player number on this PC (1 or 2) */

JE_boolean haltGame = false;

JE_boolean moveOk;

/* Special Requests */
JE_boolean pauseRequest, skipLevelRequest, helpRequest, nortShipRequest;
JE_boolean yourInGameMenuRequest, inGameMenuRequest;

#ifdef WITH_NETWORK
static void packet_copy(UDPpacket *dst, UDPpacket *src)
{
	void *temp = dst->data;
	memcpy(dst, src, sizeof(*dst));
	dst->data = temp;
	memcpy(dst->data, src->data, src->len);
}

static void packets_shift_up(UDPpacket **packet, int max_packets)
{
		if (packet[0])
		{
			SDLNet_FreePacket(packet[0]);
		}
		for (int i = 0; i < max_packets - 1; i++)
		{
			packet[i] = packet[i + 1];
		}
		packet[max_packets - 1] = NULL;
}

static void packets_shift_down(UDPpacket **packet, int max_packets)
{
	if (packet[max_packets - 1])
	{
		SDLNet_FreePacket(packet[max_packets - 1]);
	}
	for (int i = max_packets - 1; i > 0; i--)
	{
		packet[i] = packet[i - 1];
	}
	packet[0] = NULL;
}

// prepare new packet for sending
void network_prepare(Uint16 type)
{
	SDLNet_Write16(type,          &packet_out_temp->data[0]);
	SDLNet_Write16(last_out_sync, &packet_out_temp->data[2]);
}

// send packet but don't expect acknowledgment of delivery
static bool network_send_no_ack(int len)
{
	packet_out_temp->len = len;

	if (!SDLNet_UDP_Send(socket, 0, packet_out_temp))
	{
		logError("SDLNet_UDP_Send: %s", SDLNet_GetError());
		return false;
	}

	return true;
}

// send packet and place it in queue to be acknowledged
bool network_send(int len)
{
	bool temp = network_send_no_ack(len);

	Uint16 i = last_out_sync - queue_out_sync;
	if (i < NET_PACKET_QUEUE)
	{
		packet_out[i] = SDLNet_AllocPacket(NET_PACKET_SIZE);
		packet_copy(packet_out[i], packet_out_temp);
	}
	else
	{
		// connection is probably bad now
		logError("Overflowed outbound packet queue.");
		return false;
	}

	last_out_sync++;

	if (network_is_sync())
		last_out_tick = SDL_GetTicks();

	return temp;
}

// send acknowledgment packet
static int network_acknowledge(Uint16 sync)
{
	SDLNet_Write16(PACKET_ACKNOWLEDGE, &packet_out_temp->data[0]);
	SDLNet_Write16(sync,               &packet_out_temp->data[2]);
	network_send_no_ack(4);

	return 0;
}

// activity lately?
static bool network_is_alive(void)
{
	return (SDL_GetTicks() - last_in_tick < NET_TIME_OUT || SDL_GetTicks() - last_state_in_tick < NET_TIME_OUT);
}

// poll for new packets received, check that connection is alive, resend queued packets if necessary
int network_check(void)
{
	if (!net_initialized)
		return -1;

	if (connected)
	{
		// timeout
		if (!network_is_alive())
		{
			if (!quit)
				network_tyrian_halt(2, false);
		}

		// keep-alive
		static Uint32 keep_alive_tick = 0;
		if (SDL_GetTicks() - keep_alive_tick > NET_KEEP_ALIVE)
		{
			network_prepare(PACKET_KEEP_ALIVE);
			network_send_no_ack(4);

			keep_alive_tick = SDL_GetTicks();
		}
	}

	// retry
	if (packet_out[0] && SDL_GetTicks() - last_out_tick > NET_RETRY)
	{
		if (!SDLNet_UDP_Send(socket, 0, packet_out[0]))
		{
			logError("SDLNet_UDP_Send: %s", SDLNet_GetError());
			return -1;
		}

		last_out_tick = SDL_GetTicks();
	}

	switch (SDLNet_UDP_Recv(socket, packet_temp))
	{
		case -1:
			logError("SDLNet_UDP_Recv: %s", SDLNet_GetError());
			return -1;
			break;
		case 0:
			break;
		default:
			// the host takes the address of whoever connects first
			if (waiting_for_opponent && packet_temp->channel == -1 && packet_temp->len >= 4 &&
			    SDLNet_Read16(&packet_temp->data[0]) == PACKET_CONNECT)
			{
				ip = packet_temp->address;
				SDLNet_UDP_Bind(socket, 0, &ip);
				packet_temp->channel = 0;
				waiting_for_opponent = false;
				network_peer_local = address_is_local(&ip);
			}

			if (packet_temp->channel == 0 && packet_temp->len >= 4)
			{
				// Tyrian 2000+: a packet shorter than its type needs is dropped
				// (its data would be read from whatever the buffer held before)
				int minLen = 4;
				switch (SDLNet_Read16(&packet_temp->data[0]))
				{
					case PACKET_CONNECT:   minLen = 12; break;  // the name and version after it are read with their length checked
					case PACKET_DETAILS:   minLen = 8; break;
					case PACKET_STATE:
					case PACKET_STATE_XOR: minLen = NET_STATE_SIZE; break;
					case PACKET_PONG:      minLen = 8; break;
					default: break;
				}
				if (packet_temp->len < minLen)
				{
					logWarn("Received a packet of type %d with %d bytes; ignored.", SDLNet_Read16(&packet_temp->data[0]), packet_temp->len);
					return 0;
				}

				switch (SDLNet_Read16(&packet_temp->data[0]))
				{
					case PACKET_ACKNOWLEDGE:
						if ((Uint16)(SDLNet_Read16(&packet_temp->data[2]) - last_ack_sync) < NET_PACKET_QUEUE)
						{
							last_ack_sync = SDLNet_Read16(&packet_temp->data[2]);
						}

						{
							Uint16 i = SDLNet_Read16(&packet_temp->data[2]) - queue_out_sync;
							if (i < NET_PACKET_QUEUE)
							{
								if (packet_out[i])
								{
									SDLNet_FreePacket(packet_out[i]);
									packet_out[i] = NULL;
								}
							}
						}

						// remove acknowledged packets from queue
						while (packet_out[0] == NULL && (Uint16)(last_ack_sync - queue_out_sync) < NET_PACKET_QUEUE)
						{
							packets_shift_up(packet_out, NET_PACKET_QUEUE);

							queue_out_sync++;
						}

						last_in_tick = SDL_GetTicks();
						break;

					case PACKET_CONNECT:
						queue_in_sync = SDLNet_Read16(&packet_temp->data[2]);

						for (int i = 0; i < NET_PACKET_QUEUE; i++)
						{
							if (packet_in[i])
							{
								SDLNet_FreePacket(packet_in[i]);
								packet_in[i] = NULL;
							}
						}
						// fall through

					case PACKET_DETAILS:
					case PACKET_WAITING:
					case PACKET_BUSY:
					case PACKET_GAME_QUIT:
					case PACKET_GAME_PAUSE:
					case PACKET_GAME_MENU:
						{
							Uint16 i = SDLNet_Read16(&packet_temp->data[2]) - queue_in_sync;
							if (i < NET_PACKET_QUEUE)
							{
								if (packet_in[i] == NULL)
									packet_in[i] = SDLNet_AllocPacket(NET_PACKET_SIZE);
								packet_copy(packet_in[i], packet_temp);
							}
							else
							{
								// inbound packet queue overflow/underflow
								// under normal circumstances, this is okay
							}
						}

						network_acknowledge(SDLNet_Read16(&packet_temp->data[2]));
						// fall through

					case PACKET_KEEP_ALIVE:
						last_in_tick = SDL_GetTicks();
						break;

					case PACKET_QUIT:
						if (!quit)
						{
							network_prepare(PACKET_QUIT);
							network_send(4);  // PACKET_QUIT
						}

						network_acknowledge(SDLNet_Read16(&packet_temp->data[2]));

						if (!quit)
							network_tyrian_halt(1, true);
						break;

					case PACKET_STATE:
						// place packet in queue if within limits
						{
							Uint16 i = SDLNet_Read16(&packet_temp->data[2]) - last_state_in_sync + 1;
							if (i < NET_PACKET_QUEUE)
							{
								if (packet_state_in[i] == NULL)
									packet_state_in[i] = SDLNet_AllocPacket(NET_PACKET_SIZE);
								packet_copy(packet_state_in[i], packet_temp);
							}
						}
						break;

					case PACKET_STATE_XOR:
						// place packet in queue if within limits
						{
							Uint16 i = SDLNet_Read16(&packet_temp->data[2]) - last_state_in_sync + 1;
							if (i < NET_PACKET_QUEUE)
							{
								if (packet_state_in_xor[i] == NULL)
								{
									packet_state_in_xor[i] = SDLNet_AllocPacket(NET_PACKET_SIZE);
									packet_copy(packet_state_in_xor[i], packet_temp);
								}
								else if (SDLNet_Read16(&packet_state_in_xor[i]->data[0]) != PACKET_STATE_XOR)
								{
									for (int j = 4; j < packet_state_in_xor[i]->len; j++)
										packet_state_in_xor[i]->data[j] ^= packet_temp->data[j];
									SDLNet_Write16(PACKET_STATE_XOR, &packet_state_in_xor[i]->data[0]);
								}
							}
						}
						break;

					case PACKET_STATE_RESEND:
						// resend requested state packet if still available
						{
							Uint16 i = last_state_out_sync - SDLNet_Read16(&packet_temp->data[2]);
							if (i > 0 && i < NET_PACKET_QUEUE)
							{
								if (packet_state_out[i])
								{
									if (!SDLNet_UDP_Send(socket, 0, packet_state_out[i]))
									{
										logError("SDLNet_UDP_Send: %s", SDLNet_GetError());
										return -1;
									}
								}
							}
						}
						break;

					case PACKET_PING:
						SDLNet_Write16(PACKET_PONG, &packet_temp->data[0]);
						packet_temp->len = 8;
						if (!SDLNet_UDP_Send(socket, 0, packet_temp))
							logError("SDLNet_UDP_Send: %s", SDLNet_GetError());
						last_in_tick = SDL_GetTicks();
						break;

					case PACKET_PONG:
						if (packet_temp->len >= 8 && pings_received < NET_PINGS)
							ping_ms[pings_received++] = SDL_GetTicks() - SDLNet_Read32(&packet_temp->data[4]);
						last_in_tick = SDL_GetTicks();
						break;

					default:
						logWarn("Received unknown packet type %d.", SDLNet_Read16(&packet_temp->data[0]));
						return 0;
						break;
				}

				return 1;
			}
			break;
	}

	return 0;
}

// discard working packet, now processing next packet in queue
bool network_update(void)
{
	if (packet_in[0])
	{
		packets_shift_up(packet_in, NET_PACKET_QUEUE);

		queue_in_sync++;

		return true;
	}

	return false;
}

// has opponent gotten all the packets we've sent?
bool network_is_sync(void)
{
	return (queue_out_sync - last_ack_sync == 1);
}

// prepare new state for sending
void network_state_prepare(void)
{
	if (packet_state_out[0])
	{
		logWarn("Previous state packet has not been sent.");
	}
	else
	{
		packet_state_out[0] = SDLNet_AllocPacket(NET_PACKET_SIZE);
		packet_state_out[0]->len = NET_STATE_SIZE;
	}

	SDLNet_Write16(PACKET_STATE, &packet_state_out[0]->data[0]);
	SDLNet_Write16(last_state_out_sync, &packet_state_out[0]->data[2]);
	memset(&packet_state_out[0]->data[4], 0, NET_STATE_SIZE - 4);

	// Tyrian 2000+: now and then, the state this frame started from
	if (last_state_out_sync % NET_CHECK_INTERVAL == 0)
	{
		SDLNet_Write32(frame_checksum[last_state_out_sync % NET_CHECK_RING], &packet_state_out[0]->data[28]);
		packet_state_out[0]->data[32] = 1;
	}
}

void network_frame_state(Uint32 checksum)
{
	frame_checksum[last_state_out_sync % NET_CHECK_RING] = checksum;
}

// the other player's game started a frame from a different state than ours?
// (sync: the packet's number; not data[2], which a packet rebuilt from the xor
// packets has wrong)
static void check_frame_state(const UDPpacket *packet, Uint16 sync)
{
	if (packet->len < NET_STATE_SIZE || packet->data[32] != 1)
		return;

	const Uint32 theirs = SDLNet_Read32(&packet->data[28]);
	const Uint32 ours = frame_checksum[sync % NET_CHECK_RING];
	if (theirs != ours)
	{
		logError("Network game out of sync at frame %u: checksum %08x here, %08x there.", sync, ours, theirs);
		network_tyrian_halt(7, false);
	}
}

// send state packet, xor packet if applicable
int network_state_send(void)
{
	if (!SDLNet_UDP_Send(socket, 0, packet_state_out[0]))
	{
		logError("SDLNet_UDP_Send: %s", SDLNet_GetError());
		return -1;
	}

	// send xor of last network_delay packets
	if (network_delay > 1 && (last_state_out_sync + 1) % network_delay == 0 && packet_state_out[network_delay - 1] != NULL)
	{
		packet_copy(packet_temp, packet_state_out[0]);
		SDLNet_Write16(PACKET_STATE_XOR, &packet_temp->data[0]);
		for (int i = 1; i < network_delay; i++)
			for (int j = 4; j < packet_temp->len; j++)
				packet_temp->data[j] ^= packet_state_out[i]->data[j];

		if (!SDLNet_UDP_Send(socket, 0, packet_temp))
		{
			logError("SDLNet_UDP_Send: %s", SDLNet_GetError());
			return -1;
		}
	}

	packets_shift_down(packet_state_out, NET_PACKET_QUEUE);

	last_state_out_sync++;

	return 0;
}

// receive state packet, wait until received
bool network_state_update(void)
{
	if (network_state_is_reset())
	{
		return 0;
	}
	else
	{
		packets_shift_up(packet_state_in, NET_PACKET_QUEUE);

		packets_shift_up(packet_state_in_xor, NET_PACKET_QUEUE);

		last_state_in_sync++;

		// current xor packet index
		int x = network_delay - (last_state_in_sync - 1) % network_delay - 1;

		// loop until needed packet is available
		while (!packet_state_in[0])
		{
			// xor the packet from thin air, if possible
			if (packet_state_in_xor[x] && SDLNet_Read16(&packet_state_in_xor[x]->data[0]) == PACKET_STATE_XOR)
			{
				// check for all other required packets
				bool okay = true;
				for (int i = 1; i <= x; i++)
				{
					if (packet_state_in[i] == NULL)
					{
						okay = false;
						break;
					}
				}
				if (okay)
				{
					packet_state_in[0] = SDLNet_AllocPacket(NET_PACKET_SIZE);
					packet_copy(packet_state_in[0], packet_state_in_xor[x]);
					for (int i = 1; i <= x; i++)
						for (int j = 4; j < packet_state_in[0]->len; j++)
							packet_state_in[0]->data[j] ^= packet_state_in[i]->data[j];
					break;
				}
			}

			static Uint32 resend_tick = 0;
			if (SDL_GetTicks() - last_state_in_tick > NET_RESEND && SDL_GetTicks() - resend_tick > NET_RESEND)
			{
				SDLNet_Write16(PACKET_STATE_RESEND,    &packet_out_temp->data[0]);
				SDLNet_Write16(last_state_in_sync - 1, &packet_out_temp->data[2]);
				network_send_no_ack(4);  // PACKET_RESEND

				resend_tick = SDL_GetTicks();
			}

			if (network_check() == 0)
				SDL_Delay(1);
		}

		if (network_delay > 1)
		{
			// process the current in packet against the xor queue
			if (packet_state_in_xor[x] == NULL)
			{
				packet_state_in_xor[x] = SDLNet_AllocPacket(NET_PACKET_SIZE);
				packet_copy(packet_state_in_xor[x], packet_state_in[0]);
				packet_state_in_xor[x]->status = 0;
			}
			else
			{
				for (int j = 4; j < packet_state_in_xor[x]->len; j++)
					packet_state_in_xor[x]->data[j] ^= packet_state_in[0]->data[j];
			}
		}

		last_state_in_tick = SDL_GetTicks();

		check_frame_state(packet_state_in[0], (Uint16)(last_state_in_sync - 1));
	}

	return 1;
}

// ignore first network_delay states of level
bool network_state_is_reset(void)
{
	return (last_state_out_sync < network_delay);
}

// reset queues for new level
void network_state_reset(void)
{
	last_state_in_sync = last_state_out_sync = 0;

	for (int i = 0; i < NET_PACKET_QUEUE; i++)
	{
		if (packet_state_in[i])
		{
			SDLNet_FreePacket(packet_state_in[i]);
			packet_state_in[i] = NULL;
		}
	}
	for (int i = 0; i < NET_PACKET_QUEUE; i++)
	{
		if (packet_state_in_xor[i])
		{
			SDLNet_FreePacket(packet_state_in_xor[i]);
			packet_state_in_xor[i] = NULL;
		}
	}
	for (int i = 0; i < NET_PACKET_QUEUE; i++)
	{
		if (packet_state_out[i])
		{
			SDLNet_FreePacket(packet_state_out[i]);
			packet_state_out[i] = NULL;
		}
	}

	last_state_in_tick = SDL_GetTicks();
}

// the release the game is ("v1.6" of "v1.6-3-gabcdef"): two players need the
// same one, since a later version may play a level differently
static void release_version(char *buffer, size_t size)
{
	SDL_strlcpy(buffer, opentyrian_version, size);
	char *dash = strchr(buffer, '-');
	if (dash != NULL)
		*dash = '\0';
}

static int connect_packet(Uint16 episodes)
{
	char version[32];
	release_version(version, sizeof(version));

	network_prepare(PACKET_CONNECT);
	SDLNet_Write16(NET_VERSION,   &packet_out_temp->data[4]);
	SDLNet_Write16(network_delay, &packet_out_temp->data[6]);
	SDLNet_Write16(episodes,      &packet_out_temp->data[8]);
	SDLNet_Write16(thisPlayerNum, &packet_out_temp->data[10]);
	const size_t nameLen = strlen(network_player_name);
	memcpy(&packet_out_temp->data[12], network_player_name, nameLen + 1);
	memcpy(&packet_out_temp->data[12 + nameLen + 1], version, strlen(version) + 1);
	return 12 + (int)nameLen + 1 + (int)strlen(version) + 1;
}

// Esc (or B) while connecting?
static bool connect_canceled(void)
{
	push_joysticks_as_keyboard();
	handleSdlEvents();

	KeyboardInput keyboardInput;
	while (keyboardGetInput(&keyboardInput))
		if (keyboardInput.scancode == SDL_SCANCODE_ESCAPE)
			return true;
	return false;
}

// attempt to punch through firewall by firing off UDP packets at the opponent
// exchange game information
// Tyrian 2000+: without network_opponent_host, waits for anyone to connect
// (the host); returns -1 if the player pressed Esc meanwhile
int network_connect(void)
{
	waiting_for_opponent = network_opponent_host == NULL;
	if (!waiting_for_opponent)
	{
		if (SDLNet_ResolveHost(&ip, network_opponent_host, network_opponent_port) != 0)
		{
			logError("SDLNet_ResolveHost: %s", SDLNet_GetError());
			network_tyrian_halt(3, false);
			return -1;
		}

		SDLNet_UDP_Bind(socket, 0, &ip);
		network_peer_local = address_is_local(&ip);
	}

	Uint16 episodes = 0, episodes_local = 0;
	assert(EPISODE_MAX <= 16);
	for (int i = EPISODE_MAX - 1; i >= 0; i--)
	{
		episodes <<= 1;
		episodes |= (episodeAvail[i] != 0);
	}
	episodes_local = episodes;

	assert(NET_PACKET_SIZE - 12 >= 20 + 1 + 32);
	if (strlen(network_player_name) > 20)
		network_player_name[20] = '\0';

	if (waiting_for_opponent)
		goto connect_wait;

connect_reset:
	network_send(connect_packet(episodes_local)); // PACKET_CONNECT

connect_wait:
	// until opponent sends connect packet
	while (true)
	{
		setFrameCount(1);

		if (connect_canceled())
		{
			if (!network_from_menu)
				network_tyrian_halt(0, false);
			return -1;
		}

		// never timeout
		last_in_tick = SDL_GetTicks();

		if (packet_in[0] && SDLNet_Read16(&packet_in[0]->data[0]) == PACKET_CONNECT)
			break;

		network_update();

		waitUntilElapsed();
	}

	// the host answers once it knows where to
	if (last_out_sync == 0)
		network_send(connect_packet(episodes_local)); // PACKET_CONNECT

connect_again:
	if (SDLNet_Read16(&packet_in[0]->data[4]) != NET_VERSION)
	{
		logError("Network version did not match opponent's.");
		network_tyrian_halt(4, true);
		return -1;
	}
	if (thisPlayerNum == 2)
	{
		// the host chooses the delay (Tyrian 2000+: it measures the connection
		// before the first level and sends the delay with the episode)
		network_delay = SDLNet_Read16(&packet_in[0]->data[6]);
		if (network_delay < 1 || network_delay * 2 > NET_PACKET_QUEUE - 2)
			network_delay = 1 + 1;
	}
	if (SDLNet_Read16(&packet_in[0]->data[10]) == thisPlayerNum)
	{
		logError("Player number conflicts with opponent's.");
		network_tyrian_halt(6, true);
		return -1;
	}

	{
		// after the name: the version, each ending with '\0'
		const int len = packet_in[0]->len;
		const char *name = (const char *)&packet_in[0]->data[12];
		const size_t space = (size_t)(len > 12 ? len - 12 : 0);
		const char *nameEnd = memchr(name, '\0', space);
		const size_t nameLen = MIN(nameEnd != NULL ? (size_t)(nameEnd - name) : space, 20);
		char theirVersion[32] = "";
		if (nameEnd != NULL)
		{
			const size_t versionSpace = space - (size_t)(nameEnd + 1 - name);
			memcpy(theirVersion, nameEnd + 1, MIN(versionSpace, sizeof(theirVersion) - 1));
		}
		char version[32];
		release_version(version, sizeof(version));
		if (strcmp(theirVersion, version) != 0)
		{
			logError("Game version %s did not match opponent's (%.31s).", version, theirVersion);
			SDL_strlcpy(network_opponent_version, theirVersion, sizeof(network_opponent_version));
			network_tyrian_halt(8, true);
			return -1;
		}

		if (network_opponent_name != empty_string)
			free(network_opponent_name);
		network_opponent_name = malloc(nameLen + 1);
		memcpy(network_opponent_name, name, nameLen);
		network_opponent_name[nameLen] = '\0';
	}

	episodes = SDLNet_Read16(&packet_in[0]->data[8]);
	for (int i = 0; i < EPISODE_MAX; i++) {
		episodeAvail[i] &= (episodes & 1);
		episodes >>= 1;
	}

	network_update();

	// until opponent has acknowledged
	while (!network_is_sync())
	{
		setFrameCount(1);

		// got a duplicate packet; process it again (but why?)
		if (packet_in[0] && SDLNet_Read16(&packet_in[0]->data[0]) == PACKET_CONNECT)
			goto connect_again;

		// maybe opponent didn't get our packet
		if (SDL_GetTicks() - last_out_tick > NET_RETRY)
			goto connect_reset;

		waitUntilElapsed();
	}

	// send another packet since sometimes the network syncs without both connect packets exchanged
	// there should be a better way to handle this
	network_send(connect_packet(episodes_local)); // PACKET_CONNECT

	connected = true;

	return 0;
}

// Tyrian 2000+: the host times a few round trips and chooses the delay that
// keeps the game at full speed: a player's input must arrive within the delay,
// in frames of 1/35 s; the other side answers from any of its waiting loops
void network_measure_delay(void)
{
	if (network_delay_fixed)
		return;

	pings_received = 0;
	const Uint32 start = SDL_GetTicks();
	for (int sent = 0; SDL_GetTicks() - start < 2000 && pings_received < NET_PINGS; )
	{
		if (sent < NET_PINGS && SDL_GetTicks() - start >= (Uint32)sent * 100)
		{
			SDLNet_Write16(PACKET_PING, &packet_out_temp->data[0]);
			SDLNet_Write16(0, &packet_out_temp->data[2]);
			SDLNet_Write32(SDL_GetTicks(), &packet_out_temp->data[4]);
			network_send_no_ack(8);
			sent++;
		}

		if (network_check() == 0)
			SDL_Delay(1);
	}

	if (pings_received == 0)
		return;  // keep the delay

	// the second slowest trip (one may be a fluke), half of it each way, with room for jitter
	Uint32 sorted[NET_PINGS];
	memcpy(sorted, ping_ms, sizeof(Uint32) * pings_received);
	for (int i = 1; i < pings_received; i++)
		for (int j = i; j > 0 && sorted[j - 1] > sorted[j]; j--)
		{
			const Uint32 t = sorted[j - 1];
			sorted[j - 1] = sorted[j];
			sorted[j] = t;
		}
	const Uint32 rtt = sorted[pings_received >= 2 ? pings_received - 2 : 0];

	int delay = 1 + (int)((rtt / 2 + 15 + 28) / 29);  // 1000 / 35 = 28.6 ms a frame
	const int maxDelay = (NET_PACKET_QUEUE - 2) / 2;
	if (delay < 1 + 1)
		delay = 1 + 1;
	if (delay > maxDelay)
		delay = maxDelay;

	logInfo("Network: round trip %u ms (%d measured), delay %d frames.", rtt, pings_received, delay - 1);
	network_delay = delay;
}

// the network game is over: back to the title screen (or the program closes,
// if the game was started with --net); the socket is closed and the queues emptied
bool network_is_connected(void)
{
	return connected;
}

void network_close(void)
{
	for (int i = 0; i < NET_PACKET_QUEUE; i++)
	{
		SDLNet_FreePacket(packet_in[i]);
		SDLNet_FreePacket(packet_out[i]);
		packet_in[i] = packet_out[i] = NULL;
	}
	network_state_reset();
	SDLNet_FreePacket(packet_temp);
	SDLNet_FreePacket(packet_out_temp);
	packet_temp = packet_out_temp = NULL;

	if (socket != NULL)
		SDLNet_UDP_Close(socket);
	socket = NULL;

	last_out_sync = queue_in_sync = queue_out_sync = last_ack_sync = 0;
	last_in_tick = last_out_tick = 0;
	net_initialized = connected = quit = waiting_for_opponent = false;
	isNetworkGame = false;
	network_from_menu = false;
	thisPlayerNum = 0;

	SDLNet_Quit();
}

// something has gone wrong :(
void network_tyrian_halt(unsigned int err, bool attempt_sync)
{
	const char *const err_msg[] = {
		"Quitting...",
		"Other player quit the game.",
		"Network connection was lost.",
		"Network connection failed.",
		"Network version mismatch.",
		"Network delay mismatch.",
		"Network player number conflict.",
		"The two games went out of step.",
		"The other player has another version (%s).",
	};

	quit = true;

	if (err >= COUNTOF(err_msg))
		err = 0;

	char text[128];
	snprintf(text, sizeof(text), tr(err_msg[err]), network_opponent_version);

	fade_black(10);

	VGAScreen = VGAScreenSeg;

	JE_loadPic(VGAScreen, 2, false);
	JE_dString(VGAScreen, JE_fontCenter(text, SMALL_FONT_SHAPES), 140, text, SMALL_FONT_SHAPES);

	JE_showVGA();
	fade_palette(colors, 10, 0, 255);

	if (attempt_sync)
	{
		while (!network_is_sync() && network_is_alive())
		{
			setFrameCount(1);

			waitUntilElapsed();
		}
	}

	if (err)
		waitUntilGetInput();

	fade_black(10);

	if (network_abort != NULL && network_from_menu)
	{
		network_close();
		longjmp(*network_abort, 1);
	}

	SDLNet_Quit();

	JE_tyrianHalt(5);
}

int network_init(void)
{
	logInfo("Initializing network...");

	if (network_delay * 2 > NET_PACKET_QUEUE - 2)
	{
		logError("Network delay would overflow packet queue.");
		return -4;
	}

	if (SDLNet_Init() != 0)
	{
		logError("SDLNet_Init: %s", SDLNet_GetError());
		return -1;
	}

	// the one joining takes any free port: the host answers wherever it came from
	socket = SDLNet_UDP_Open(network_from_menu && network_opponent_host != NULL ? 0 : network_player_port);
	if (!socket)
	{
		logError("SDLNet_UDP_Open: %s", SDLNet_GetError());
		SDLNet_Quit();
		return -2;
	}

	packet_temp = SDLNet_AllocPacket(NET_PACKET_SIZE);
	packet_out_temp = SDLNet_AllocPacket(NET_PACKET_SIZE);

	if (!packet_temp || !packet_out_temp)
	{
		logError("SDLNet_AllocPacket: %s", SDLNet_GetError());
		return -3;
	}

	net_initialized = true;

	return 0;
}

#endif

void JE_clearSpecialRequests(void)
{
	pauseRequest = false;
	inGameMenuRequest = false;
	skipLevelRequest = false;
	helpRequest = false;
	nortShipRequest = false;
}
