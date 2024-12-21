import socket
import time
import threading
import sys
import csv
import os
import signal
from collections import namedtuple

USER_FILE = "lobby/server_users.csv"
GAME_FILE = "lobby/server_games.csv"

Player = namedtuple('Player', ['username', 'password', 'status'])
Game = namedtuple('Game', ['name', 'owner', 'intro'])
Room = namedtuple('Room', ['name', 'owner', 'game_name', 'ip', 'port', 'publicity', 'status'])

# Global data structures protected by a lock
lock = threading.Lock()
username_to_socket = {} #Format: {username: sock}
players = {} # Format: {"username": "password"}
players_ip = {} # Format: {"username": "ip"}
current_player = {} # Format: {"sock": username}
player_room = {} # Format: {username: room}
start_game = {} # Format: {owner: 0: Not yet/ 1: can start}
rooms = [] 
invited_list = {} # Format: {"username": [{name, owner, gtype, ip}]}
Game = {} # Format: {"game name": {name, owner, intro}}
publish_game = {} # Format: {"username": [{name, owner, intro}]}

room_status = ["Waiting", "Full"]
player_status = ["Idle", "In Room"]

welcome_message = ("Welcome! Game client running...\n"
                   "Please select an option:\n"
                   "1. Register\n"
                   "2. Login\n"
                   "3. Exit\n"
                   "Your option: ")

def load_users_from_file():
    """Load users from a CSV file into the players dictionary."""
    if not os.path.exists(USER_FILE):
        return
    with open(USER_FILE, mode="r", encoding="utf-8") as file:
        reader = csv.DictReader(file)
        for row in reader:
            username = row["username"]
            password = row["password"]
            players[username] = Player(username=username, password=password, status=0)

def load_games_from_file():
    if not os.path.exists(GAME_FILE):
        return
    with open(GAME_FILE, mode="r", encoding="utf-8") as file:
        reader = csv.DictReader(file)
        for row in reader:
            game_name = row["name"]
            owner = row["owner"]
            intro = row["intro"]
            Game[game_name] = {'name': game_name, 'owner': owner, 'intro':intro}
            if owner not in publish_game:
                publish_game[owner] = publish_game[owner] = {}
            publish_game[owner][game_name] = {'name': game_name, 'owner': owner, 'intro':intro}

def is_socket_open(sock):
    try:
        sock.getpeername()  # Check if the socket is connected
        return True
    except socket.error:
        return False

def send_msg(sock, message):
    sock.send(message.encode('utf-8'))

def recv_msg(sock):
    try:
        data = sock.recv(1024)
        if not data:
            return None
        return data.decode('utf-8', errors='ignore').strip()
    except:
        return None

def format_table(headers, rows):
    # Generate table header
    table = "===".join(['=' * len(header) for header in headers]) + "\n"
    table += " | ".join(headers) + "\n"
    table += "===".join(['=' * len(header) for header in headers]) + "\n"
    
    # Generate rows
    for row in rows:
        table += " | ".join(str(cell).ljust(len(header)) for cell, header in zip(row, headers)) + "\n"
        table += "===".join(['=' * len(header) for header in headers]) + "\n"
    
    return table

def handle_register(sock):
    while True:
        response = recv_msg(sock)
        #print(response)
        try:
            if "username:{" not in response or "password:{" not in response:
                send_msg(sock, "Invalid format\n")
                #print("h1")
                continue
        except Exception as e:
            print(f"Exception occurred: {e}")
        try:
            user_part = response.split("username:{")[1].split("}")[0]
            pass_part = response.split("password:{")[1].split("}")[0]
            user = user_part.strip()
            passwd = pass_part.strip()
        except:
            send_msg(sock, "Invalid format\n")
            #print("h2")
            continue
        try:
            if user in players:
                send_msg(sock, "user already exist\n")
                #print("h3")
            else:
                with lock:
                    p = Player(username=user, password=passwd, status=0)
                    players[user] = p
                    file_exists = os.path.exists(USER_FILE)
                    with open(USER_FILE, mode="a", newline="", encoding="utf-8") as file:
                        writer = csv.writer(file)
                        if not file_exists:
                            writer.writerow(["username", "password"])  # Header row
                        writer.writerow([user, passwd])
                send_msg(sock, "REGISTER OK")
                #print("h4")
                break
        except Exception as e:
            print(f"Exception occurred: {e}")

def handle_login(sock, addr):
    # Ask for username
    #print("hello")
    while True:
        user = recv_msg(sock)
        if user not in players:
            send_msg(sock, "User doesn't exist\n")
        else:
            send_msg(sock, "User exist\n")
            break
    # Ask for password
    while True:
        passwd = recv_msg(sock)
        if players[user].password != passwd:
            send_msg(sock, "Wrong password\n")
        else:
            # Login success
            # Update status
            with lock:
                p = players[user]._replace(status=0)
                players[user] = p
                players_ip[user] = addr
                current_player[sock] = user
                username_to_socket[user] = sock
            send_msg(sock, "Login success\n")
            break
    try:
        for user in current_player.values():
            if players[user].status == 0 and current_player[sock] != user:
                send_msg(username_to_socket[user], f"Player: {current_player[sock]} join the lobby server")
    except Exception as e:
            print(f"Login Error: {e}")


def In_room(sock, idx):
    msg = ""
    while True:
        if current_player[sock] == rooms[idx].owner:
            with lock:
                start_game[current_player[sock]] = 0
            msg += ("In room.... ( Role: Host )\n"
                    "Please select an option:\n"
                    "1. Invite player\n"
                    "2. Start the Game\n"
                    "3. Back to lobby\n"
                    "Your option: ")
            send_msg(sock, msg)
            option = recv_msg(sock)
            print(f"h1: {option}")
            if option == '1':
                msg = ""
                while True:
                    msg += ("Please select an option:\n"
                        "1. List all idle player\n"
                        "2. Send invitation\n"
                        "3. Back to room\n"
                        "Your option: ")
                    send_msg(sock, msg)
                    option2 = recv_msg(sock)
                    print(f"h2: {option2}")
                    idle_players = [user for user in current_player.values() if players[user].status == 0]
                    if option2 == '1':
                        message = "All Idle users:\n"
                        message += "=================================\n"
                        # Prepare online players
                        if idle_players:
                            for count, username in enumerate(idle_players, start=1):
                                message += f"{count}. {username}\n"
                        else: 
                            message += "No idle user available to invite.\n"
                        message += "=================================\n"
                        send_msg(sock, message)
                        msg = ""
                    elif option2 == '2':
                        message = "Idle users:\n"
                        message += "=================================\n"
                        # Prepare online players
                        if idle_players:
                            for count, username in enumerate(idle_players, start=1):
                                message += f"{count}. {username}\n"
                        else:
                            message += "No idle user available to invite.\n"
                        message += "=================================\n"
                        msg = message
                        while True:
                            try:
                                msg += "Enter the number of player to invite: "
                                send_msg(sock, msg)
                                while True:
                                    user_idx = recv_msg(sock)
                                    user_idx = int(user_idx)
                                    if user_idx <= 0 or user_idx > len(idle_players):
                                        msg = ("=========================\n"
                                                "Invalid input, try again.\n"
                                                "=========================\n")
                                        msg += "Enter the number of player to invite: "
                                        send_msg(sock, msg)
                                    else:
                                        break
                                invited_player = idle_players[user_idx - 1]
                                invited_player_conn = username_to_socket[invited_player]
                            except Exception as e:
                                print(f"Error: {e}")
                            if players[invited_player].status == 1:
                                msg = f"{invited_player} is not idle. Please choose another user to invite.\n"
                            else:
                                break
                        # Add the invitation to the invited player's list
                        try:
                            print(idx)
                            print(rooms[idx])
                            invitation = {
                                "room_name": rooms[idx].name,
                                "owner": current_player[sock],
                                "game_name": rooms[idx].game_name,
                                "ip": rooms[idx].ip
                            }
                            with lock:
                                if invited_player not in invited_list:
                                    invited_list[invited_player] = []
                                invited_list[invited_player].append(invitation)
                        except Exception as e:
                                print(f"HError: {e}")
                        try:
                            send_msg(invited_player_conn, f"You have been invited to join a private room by {current_player[sock]}. Game type: {rooms[idx].game_name}.\nCheck invitations to join.")
                        except Exception as e:
                                print(f"Error: {e}")
                        msg = ""
                    elif option2 == '3':
                        break
                    else:
                        msg = ("=========================\n"
                                "Invalid input, try again.\n"
                                "=========================\n")
                msg = ""
            elif option == '2':
                if rooms[idx].status == 1:
                    msg = ""
                    while True:
                        #print("check")
                        msg += "Please enter the port number to bind(10000-65535): "
                        send_msg(sock, msg)
                        port = recv_msg(sock)
                        msg = f"START_GAME {rooms[idx].ip} {port} Host"
                        send_msg(sock, msg)
                        check = recv_msg(sock)
                        if check == "game server established":
                            break
                        else:
                            msg = "Can't not bind the TCP port, try another\n"
                    with lock:
                        r = rooms[idx]
                        rooms[idx] = r._replace(port=port)
                        start_game[current_player[sock]] = 1
                    break
                msg = ("======================================\n"
                       "Only one player can not play the game.\n"
                       "======================================\n")
            elif option == '3':
                send_msg(sock, "LEAVE")
                with lock:
                    owner = current_player[sock]
                    p = players[owner]._replace(status=0)
                    players[owner] = p
                    player_room[rooms[idx].name].remove(owner)
                    if rooms[idx].status == 1:
                        r = rooms[idx]
                        rooms[idx] = r._replace(owner=player_room[rooms[idx].name][0], ip=players_ip[player_room[rooms[idx].name][0]][0], status=0)
                    else:
                        del rooms[idx]
                break
            else:
                msg = ("=========================\n"
                       "Invalid input, try again.\n"
                       "=========================\n")
        else:
            msg += ("In room.... ( Role: Player )\n"
                    "Please select an option:\n"
                    "1. Request to start\n"
                    "2. Back to lobby\n"
                    "Your option: ")
            send_msg(sock, msg)
            option = recv_msg(sock)
            if option == '1':
                try:
                    if start_game[rooms[idx].owner] == 1:
                        msg = f"START_GAME {rooms[idx].ip} {rooms[idx].port} Player"
                        send_msg(sock, msg)
                        break
                except Exception as e:
                    print(f"YError: {e}")
                msg = ("=================================\n"
                       "Host not ready to start the game.\n"
                       "=================================\n")
            elif option == '2':
                send_msg(sock, "LEAVE")
                with lock:
                    owner = current_player[sock]
                    p = players[owner]._replace(status=0)
                    players[owner] = p
                    player_room[rooms[idx].name].remove(owner)
                    r = rooms[idx]
                    rooms[idx] = r._replace(status=0)
                break
            else:
                msg = ("=========================\n"
                       "Invalid input, try again.\n"
                       "=========================\n")
    print("Leave the room")
    

def handle_create_room(sock):
    resp = recv_msg(sock)
    #print(resp)
    name, game_name, ip_str, pub_str= resp.split()
    pub = int(pub_str)
    with lock:
        owner = current_player[sock]
        p = players[owner]._replace(status=1)
        players[owner] = p
        player_room[name] = []
        player_room[name].append(owner)
        new_room = Room(name=name, owner=owner, game_name=game_name, ip=ip_str, port='0', publicity=pub, status=0)
        rooms.append(new_room)
    try:
        for user in current_player.values():
            if players[user].status == 0 and current_player[sock] != user and pub == 1:
                send_msg(username_to_socket[user], f"Public Room: {name}, Game type: {game_name} just be created")
    except Exception as e:
            print(f"Create Error: {e}")
    idx = -1
    for i, room in enumerate(rooms):
        if room.name == name:
            idx = i
            break
    In_room(sock, idx)

def handle_upload(sock, filename):
    os.makedirs("lobby/game", exist_ok=True)
    with lock:
        try:
            file_path = "lobby/game/"+filename+".py"
            with open(file_path, "wb") as f:
                while True:
                    # Receive file data
                    data = sock.recv(1024)
                    if b"EOF" in data:  # End of file indicator
                        data, _ = data.split(b"EOF", 1)
                        #print(f"Writing chunk: {data[:20]}...")
                        f.write(data)
                        break
                    #print(f"Writing chunk: {data[:20]}...")
                    f.write(data)
        except Exception as e:
            print(f"Error: {e}")
    print(f"File {file_path} received successfully.")

def handle_download(sock, filename):
    file_path = "lobby/game/"+filename+".py"
    with lock:
        if os.path.exists(file_path):
            filesize = os.path.getsize(file_path)
            send_msg(sock, f"CHECK {filesize}")
            time.sleep(0.1)
            # Open and send file data
            with open(file_path, "rb") as f:
                while (chunk := f.read(1024)):
                    sock.send(chunk)
            sock.send(b"EOF")  # End of file indicator
            print(f"File {file_path} sent successfully.")
        else:
            send_msg(sock, "ERROR: File not found.")

def handle_join_room(sock, addr):
    while True:
        room_name = recv_msg(sock)
        room_owner = ""
        if room_name == "exit":
            return
        idx = -1
        for i, r in enumerate(rooms):
            if r.name == room_name and r.status == 0 and r.publicity == 1:
                idx = i
                room_owner = r.owner
                break
        if idx == -1:
            send_msg(sock, "==============================\nroom not exit or not available\n==============================\n")
        else:
            with lock:
                r = rooms[idx]
                rooms[idx] = r._replace(status=1)
                user = current_player[sock]
                player_room[rooms[idx].name].append(current_player[sock])
                p = players[user]._replace(status=1)
                players[user] = p
            response = f"{rooms[idx].game_name}"
            send_msg(sock, response)
            break
    handle_download(sock, rooms[idx].game_name)
    time.sleep(0.1)
    In_room(sock, idx)

def handle_client(sock, addr):
    # Initial menu: REGISTER, LOGIN, EXIT
    #send_msg(sock, welcome_message)
    logged_in = False
    # After login: show online players and rooms
    try:
        while True:
            while not logged_in:
                cmd = recv_msg(sock)
                if cmd == "REGISTER":
                    handle_register(sock)
                elif cmd == "LOGIN":
                    handle_login(sock, addr)
                    logged_in = True
                elif cmd == "EXIT":
                    with lock:
                        if sock in current_player:
                            user = current_player[sock]
                            players[user] = players[user]._replace(status=0)
                            del current_player[sock]
                            del username_to_socket[user]
                    sock.close()
                    return
                else:
                    send_msg(sock, "Unknown command")
            # Wait for commands: LOGOUT, LIST, CREATE, JOIN, END_GAME
            #send_msg(sock, "Please select an option:\n1. Logout\n2. List rooms\n3. Create room\n4. Join room\nYour option: ")
            cmd = recv_msg(sock)
            #print(cmd)
            if cmd == "LOGOUT":
                try:
                    for user in current_player.values():
                        if players[user].status == 0 and current_player[sock] != user:
                            send_msg(username_to_socket[user], f"Player: {current_player[sock]} leave the lobby server")
                except Exception as e:
                        print(f"Logout Error: {e}")
                with lock:
                    if sock in current_player:
                        user = current_player[sock]
                        players[user] = players[user]._replace(status=0)
                        del current_player[sock]
                        del username_to_socket[user]
                # Return to initial stage
                logged_in = False
            elif cmd == "LIST_ROOM":
                message = ""
                # Prepare rooms
                room_table = []
                for r in rooms:
                    room_table.append([
                        r.name,
                        r.game_name,
                        "Public" if r.publicity == 1 else "Private",
                        room_status[r.status],
                        r.owner
                    ])
                if not room_table:
                    message = ("===========================\n"
                               "No rooms exits for players.\n"
                               "===========================\n")
                else:
                    message = "\nGame Rooms:\n" + format_table(["Room Name    ", "Game Type          ", "Room Type   ", "Status", "Owner    "], room_table)
                send_msg(sock, message)
            elif cmd == "LIST_PLAYER":
                message = ""
                # Prepare online players
                player_table = []
                for s, u in current_player.items():
                    if s == sock:
                        continue
                    player_table.append([players_ip[u], u, player_status[players[u].status]])
                if not player_table:
                    message = ("=================================\n"
                               "Currently, no players are online.\n"
                               "=================================\n")
                else:
                    message = "\nOnline players:\n" + format_table(["Address                    ", "Username", "Status"], player_table)
                send_msg(sock, message)
            elif cmd == "CREATE":
                handle_create_room(sock)
            elif cmd == "JOIN":
                handle_join_room(sock, addr)
            elif cmd.startswith("UPLOAD"):
                _, filename, intro = cmd.split()
                print(cmd)
                with lock:
                    try:
                        file_exists = os.path.exists(GAME_FILE)
                        with open(GAME_FILE, mode="a", newline="", encoding="utf-8") as file:
                            writer = csv.writer(file)
                            if not file_exists:
                                writer.writerow(["name", "owner", "intro"])  # Header row
                            writer.writerow([filename, current_player[sock], intro])
                        Game[filename] = {'name': filename, 'owner': current_player[sock], 'intro':intro}
                        if current_player[sock] not in publish_game:
                            publish_game[current_player[sock]] =  {}
                        publish_game[current_player[sock]][filename] = {'name': filename, 'owner': current_player[sock], 'intro': intro}
                    except Exception as e:
                        print(f"Error: {e}")
                time.sleep(0.1)
                handle_upload(sock, filename)
            elif cmd.startswith("DOWNLOAD"):
                _, filename = cmd.split()
                handle_download(sock, filename)
            elif cmd == "SHOW_GAME":
                game_table = []
                for _, value in Game.items():
                        game_table.append([value['name'],
                                          value['owner'],
                                          value['intro']])
                if not game_table:
                    msg = ("================================\n"
                            "No game have been published yet.\n"
                            "================================\n")
                else:
                    msg = f"All available games:\n"+format_table(["Game Name                ", "Developer    ", "Introduction        "], game_table)
                send_msg(sock, msg)
            elif cmd == "SHOW_GAME2":
                msg = ""
                if current_player[sock] not in publish_game or not publish_game[current_player[sock]]:
                    msg = ("================================\n"
                            "No game have been published yet.\n"
                            "================================\n")
                else:
                    game_table = []
                    for _, value in publish_game[current_player[sock]].items():
                        game_table.append([value['name'],
                                          value['owner'],
                                          value['intro']])
                        msg = f"{current_player[sock]}'s published games:\n"+format_table(["Game Name                ", "Developer    ", "Introduction        "], game_table)
                send_msg(sock, msg)

            elif cmd == "SHOW_INVITE":
                if current_player[sock] not in invited_list or not invited_list[current_player[sock]]:
                    msg = ("Pending Invitations:\n"
                           "=======================\n"
                           "No pending invitations.\n"
                           "=======================\n")
                    send_msg(sock, msg)
                    continue
                try:
                    invitation_list = []
                    for count, invite in enumerate(invited_list[current_player[sock]], start=1):
                        invitation_list.append([f"{count}.",
                                                invite['room_name'],
                                                invite['game_name'],
                                                invite['owner'],
                                                invite['ip']])
                    msg = "Pending Invitations:\n" + format_table(["index", "Room Name    ", "Game Name          ", "Inviter   ", "Ip_address    "], invitation_list)
                    send_msg(sock, msg)
                except Exception as e:
                        print(f"Error: {e}")
                while True:
                    option = recv_msg(sock)
                    option = int(option)
                    if option == 0:
                        break
                    elif option < 0 and option > len(invitation_list):
                        send_msg(sock, "Invalid")
                    else:
                        send_msg(sock, "Valid")
                        break
                if option == 0:
                    continue
                try:
                    selected_invite = invited_list[current_player[sock]][option - 1]
                    #room_owner = selected_invite['owner']
                except Exception as e:
                        print(f"Error: {e}")
                msg = recv_msg(sock)
                if msg == "Accept":
                    idx = -1
                    for i, room in enumerate(rooms):
                        if room.name == selected_invite['room_name']:
                            idx = i
                            break
                    if rooms[idx].status == 1:
                        send_msg(sock, "==============================\nroom not exit or not available\n==============================\n")
                    else:
                        with lock:
                            r = rooms[idx]
                            rooms[idx] = r._replace(status=1)
                            p = players[current_player[sock]]._replace(status=1)
                            players[current_player[sock]] = p
                            player_room[selected_invite['room_name']].append(current_player[sock])
                            invited_list[current_player[sock]].remove(selected_invite)
                        send_msg(sock, f"{rooms[idx].game_name}")
                        handle_download(sock, rooms[idx].game_name)
                        time.sleep(0.1)
                        In_room(sock, idx)
                else:
                    with lock:
                        invited_list[current_player[sock]].remove(selected_invite)

            elif cmd.startswith("END_GAME"):
                # Remove the room owned by user after game ends
                find_name = cmd.split(" ", 1)[1]
                with lock:
                    for i, r in enumerate(rooms):
                        if r.owner == find_name:
                            players[find_name] = players[find_name]._replace(status=0)
                            del rooms[i]
                            break
                    if find_name in players:
                        players[find_name] = players[find_name]._replace(status=0)
    except:
        # Clean up on disconnect
        with lock:
            if sock in current_player:
                user = current_player[sock]
                players[user] = players[user]._replace(status=0)
                del current_player[sock]
        sock.close()


def main():
    # Start server
    load_users_from_file()
    load_games_from_file()
    tcp_port = 0
    server_sock = None
    while server_sock is None:
        tcp_port = int(input("Please enter the server port(13400-13405):"))
        try:
            server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            server_sock.bind(('140.113.235.151', tcp_port))
            server_sock.listen(10)
            print(f"TCP server listening on Port: {tcp_port}")
            break
        except ValueError:
            print("Invalid port number. Please try again.")
        except KeyboardInterrupt:
            print("\nServer shut down by key interupt\n")
        except socket.error as e:
            print(f"Error creating or binding server socket: {e}")
            server_sock = None
    try:
        while server_sock:
            try:
                client, addr = server_sock.accept()
                t = threading.Thread(target=handle_client, args=(client, addr))
                t.start()
            except Exception as e:
                print(f"Error accepting connection: {e}")
    except KeyboardInterrupt:
        print("\nServer shut down by key interupt")
    finally:
        if server_sock:
            server_sock.close()
        print("TCP server socket closed.")
    

if __name__ == "__main__":
    main()
