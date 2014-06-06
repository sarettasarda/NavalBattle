Naval Battle
===========
Naval Battle is a client/server application based on peer2peer paradigm. 
The server and the client are mono-process and use the multiplexing I/O (api _select) to manage simultaneously the input and the output.

The server memorizes the clients connected and the ports where they are listening.

The map is 10x10 and the coordinates are [A-J][0-9] based (for example C4).
Each player have 4 boats:
- 1 of length 6
- 1 of length 4
- 1 of length 3
- 1 of length 2

Client and server communicate with pieces of information using a TCP socket. This pieces of information are used to implement the p2p communication. The text comunication from client to client is made by UDP socket.

How to run it:
===========
###Server
1. compile the server: `gcc naval_battle_server.c -o naval_battle_server`
2. run the server: `./naval_battle_server <host> <port>`

* `<host>` is the host address were the server is running,
* `<port>` is the port were the server are listening


###Client
0. be sure that the server is running
1. compile the client: `gcc naval_battle_client.c -o naval_battle_client`
2. run the client: `./naval_battle_client <remote host> <port>`
3. insert your name
4. insert the UDP port
5. insert the coordinates and the orientation for each of the 4 boats.

- `<remote host>` is the host address were the server is running,
- `<port>` is the port were the server are listening

Client commands:
===========
#### help:
shows the commands list

    >!help
    * !help -> shows the commands list
    * !who -> shows the list of the clients connetted with the server
    * !connect client_name -> starts a game with the peer client_name
    * !disconnect ->closes the game with the other peer
    * !show_enemy_map -> shows the enemy  map
    * !show_my_map -> shows the own map
    * !hit coordinates -> hits the coordinates 'coordinates'
    * !quit -> disconnects from the server

####who 
shows the list of the clients connetted with the server

    > !who
    Clients connected: client1 client2 client3 

####connect client_name 
starts a game with the client client_name: the client asks at the server (with TCP) if the client client_name exists. If client_name exists and is not busy in another game, the server sends at client_name a request to start a game. If client_name accept, the server will send the ip address and the UDP port of client_name to the client. 

    >!connect client_name
    client_name accepted your challenge.
    Game started with client_name.


When the game starts the first char of the shell will change from '>' to '#'.

####disconnect 
closes the game with the other client: the client send is state at the server (with TCP) and at the other client (with UDP) that will close the connection.

    #!disconnect
    You are now disconnected: YOU LOSE BECAUSE YOU CLOSE THE GAME

####quit
disconnects from the server: the client closes the TCP socket with the server, the UDP socket and exits.

    >!quit 
    You are now disconnected.

####show_my_map 
shows the client map wit the position of the boats and the hitches from the enemy

![My image](https://raw.githubusercontent.com/sarettasarda/NavalBattle/master/img/show_my_map.jpg)

####show_enemy_map 
shows the enemy  map with the hitches from the client


####hit coordinates
hits the coordinates 'coordinates'in the enemy map

    It's your turn:
    #!hit A5
    client_name says: HITTED
    It's client_name turn

Credits
=============
Developed by Sara Craba (sarettasarda)


License
=============

    Copyright (C) 2013 Sara Craba

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.

	Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

	- Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
	- Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
	- Neither the name of the <ORGANIZATION> nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.	
