import socket
import sys
import os
import time
import getpass
import threading
import importlib.util

game = ["Rock Paper Sciccer", "Tic-tac-toe", "Game3", "Gmae4"]

host_ips = {"linux1": "140.113.235.151", 
            "linux3": "140.113.235.153",
            "linux2": "140.113.235.152",
            "linux4": "140.113.235.154"}

host = host_ips[socket.gethostname()]

def send_msg(sock, message):
    sock.send(message.encode('utf-8'))

def recv_msg(sock):
    try:
        data = sock.recv(1024)
        if not data:
            return None
        return data.decode('utf-8', errors='ignore').strip()
    except Exception as e:
        return None
    
def message_receiver(sock, stop_event):
    while True:
        try:
            while not stop_event.is_set():
                message = recv_msg(sock)
                if message:
                    print(f"\n[Server]: {message}")
                    print("Your option: ", end="", flush=True)  # Reprint prompt after a message
        except socket.error:
            break
        time.sleep(0.1)  # Avoid busy-waiting

def Register(sock):
    send_msg(sock, "REGISTER")
    # The server will ask for username/password format
    while True:
        user = input("Please enter your username: ")
        passwd = getpass.getpass("Please enter your password: ")
        msg = f"username:{{{user}}}, password:{{{passwd}}}\n"
        send_msg(sock, msg)
        resp = recv_msg(sock)
        print(resp)
        if "REGISTER OK" in resp:
            break

def Login(sock):
    send_msg(sock, "LOGIN")
    while True:
        user = input("Please enter your username:")
        send_msg(sock, user)
        resp = recv_msg(sock)
        if resp is None:
            return
        resp = resp.strip()
        #print(resp)
        if "User doesn't exist" not in resp:
            break

    while True:
        passwd = getpass.getpass("Please enter your password:")
        send_msg(sock, passwd)
        resp = recv_msg(sock)
        if resp is None:
            return
        resp = resp.strip()
        print(resp)
        if "Login success" in resp:
            break
    global username
    username = user

def Exit(sock):
    send_msg(sock, "EXIT")
    sock.close()

def Logout(sock):
    send_msg(sock, "LOGOUT")

def List_room(sock):
    send_msg(sock, "LIST_ROOM")
    resp = recv_msg(sock)
    if resp:
        print(resp)

def List_player(sock):
    send_msg(sock, "LIST_PLAYER")
    resp = recv_msg(sock)
    if resp:
        print(resp)

def Show_game(sock):
    send_msg(sock, "SHOW_GAME")
    resp = recv_msg(sock)
    if resp:
        print(resp)

def In_room(sock, game_module):
    global username
    while True:
        msg = recv_msg(sock)
        if msg == "LEAVE":
            print("Leave the room")
            return
        elif "room not exit" in msg:
            print(msg)
            return
        elif msg.startswith("START_GAME"):
            _, server_ip, port, role = msg.split()
            try:
                if role == "Host":
                    game_server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    game_server.bind((server_ip, int(port)))
                    game_server.listen(2)
                    send_msg(sock, "game server established")
                    break
                else:
                    print("hello")
                    break
            except:
                send_msg(sock, "game server error")
        elif "All Idle users:" in msg:
            print(msg)
        else:
            print(msg, end=" ")
            option = input()
            send_msg(sock, f"{option}")
    print(f"Game server address: {server_ip}:{port}")
    print(f"Game staring....")
    if role == "Host":
        conn, addr = game_server.accept()
        print("Welcome!")
        game_module.start_game(conn, role)
        conn.close()
        game_server.close()
    else:
        conn = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        conn.connect((server_ip, int(port)))
        print("Welcome!")
        game_module.start_game(conn, role)
        conn.close()
    # After "game" ends:
    send_msg(sock, f"END_GAME {username}\n")
    print("Game end")

def Create(sock):
    name = input("Please enter the room name: ")
    pub = int(input("Is this room public?(1:Yes, 2:No): "))
    Show_game(sock)
    while True:
        game_name = input("Please enter the game name to play: ")
        send_msg(sock, f"DOWNLOAD {game_name}")
        metadata = recv_msg(sock)
        if metadata.startswith("ERROR"):
            print(metadata)
        else:
            _, filesize = metadata.split()
            download_file(sock, game_name, filesize)
            break
    #port = int(input("Please enter the port number to bind(10000-65535): "))
    # Setup a temporary game server socket for demonstration:
    # In a full solution, you'd run a mini-server here. For now, we just send the info to server.
    send_msg(sock, "CREATE")
    msg = f"{name} {game_name} {host} {pub}\n"
    send_msg(sock, msg)
    global username
    module_path = f"{username}/download/" + game_name + ".py"
    spec = importlib.util.spec_from_file_location(game_name, module_path)
    if spec is None:
        print(f"Error: Could not load the game module from {module_path}.")
    game_module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(game_module)
    In_room(sock, game_module)

def Join(sock):
    send_msg(sock, "JOIN")
    while True:
        name = input("Please enter the room name or 'exit' to quit: ")
        send_msg(sock, name)
        if name == "exit":
            return
        resp = recv_msg(sock)
        if "room not exit or not available" in resp:
            print(resp)
        else:
            break
    metadata = recv_msg(sock)
    _, filesize = metadata.split()
    download_file(sock, resp, filesize)
    global username
    module_path = f"{username}/download/" + resp + ".py"
    spec = importlib.util.spec_from_file_location(resp, module_path)
    game_module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(game_module)
    print(module_path)
    In_room(sock, game_module)

def Show_invite(sock):
    send_msg(sock, "SHOW_INVITE")
    resp = recv_msg(sock)
    print(resp)
    while True:
        option = input("Enter the number of the invitation to reply or 0 to exit: ")
        send_msg(sock, option)
        if option == '0':
            return
        resp = recv_msg(sock)
        if "Invalid" == resp:
            print("Invalid option, try again")
        else:
            break
    while True:
        option = input("Do you want to join this room? (Y/N): ")
        if option == 'y' or option == 'Y':
            send_msg(sock, "Accept")
            break
        elif option == 'n' or option == 'N':
            send_msg(sock, "Reject")
            return
        else:
            print("Invalid input, try again.")
    msg = recv_msg(sock)
    if "room not exit" in msg:
            print(msg)
            return
    else:
        filename = msg
    metadata = recv_msg(sock)
    _, filesize = metadata.split()
    download_file(sock, filename, filesize)
    global username
    module_path = f"{username}/download/" + filename + ".py"
    spec = importlib.util.spec_from_file_location(filename, module_path)
    game_module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(game_module)
    In_room(sock, game_module)

def download_file(sock, filename, filesize):
    try:
        os.makedirs(f"{username}/download", exist_ok=True)
        filesize = int(filesize)
        file_path = f"{username}/download/" + filename + ".py"
        with open(file_path, "wb") as f:
            while True:
                data = sock.recv(1024)
                if b"EOF" in data:  # End of file indicator
                    data, _ = data.split(b"EOF", 1)
                    #print(f"Writing chunk: {data[:20]}...")
                    f.write(data)
                    break
                f.write(data)
        print(f"{file_path} download successfully.")
    except Exception as e:
        print(f"download error: {e}")

def Game_dev(sock):
    global username
    while True:
        print("Please select an option:")
        print("1. List your game")
        print("2. publish the game")
        print("3. Back to lobby")
        option = input("Your option: ")
        if option == '1':
            send_msg(sock, "SHOW_GAME2")
            msg = recv_msg(sock)
            print(msg)
        elif option == '2':
            while True:
                filename = input("Enter your filename(ignore .py): ")
                file_path = f"{username}/game/" + filename + ".py"
                file_exists = os.path.exists(file_path)
                if not file_exists:
                    print("Not found file, try again.")
                else:
                    break
            intro = input("Introduction of your game: ")
            send_msg(sock, f"UPLOAD {filename} {intro}")
            time.sleep(0.1)
            try:
                filesize = os.path.getsize(file_path)
                with open(file_path, "rb") as f:
                    while (chunk := f.read(1024)):
                        #print(f"Sending: {chunk[:20]}... ({len(chunk)} bytes)")
                        sock.send(chunk)
                #print(f"Sending: EOF)")
                sock.send(b"EOF")
                print(f"{file_path} upload successfully.")
            except Exception as e:
                print(f"Upload Error: {e}")

        elif option == '3':
            return
        else:
            print("Option out of range!")


def main():
    while True:
        try:
            tcp_port = int(input("Please enter the server port(13400-13405):"))
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect(('140.113.235.151', tcp_port))
            break
        except ValueError:
            print("Invalid port number. Please try again.")
        except KeyboardInterrupt:
            print("\nClient shut down by key interupt\n")
            return 
        except socket.error as e:
            print(f"Failed to connect to TCP server: {e}")

    print("Welcome! Game client running...")
    stop_event = threading.Event()
    stop_event.set()
    message_thread = threading.Thread(target=message_receiver, args=(sock, stop_event))
    message_thread.daemon = True
    message_thread.start()
    original_timeout = sock.gettimeout()
    login = False
    while True:
        while not login:
            print("Please select an option:")
            print("1. Register")
            print("2. Login")
            print("3. Exit")
            option = input("Your option: ")
            if option == '1':
                Register(sock)
            elif option == '2':
                Login(sock)
                login = True
            elif option == '3':
                Exit(sock)
                return
            else:
                print("Option out of range!")
        sock.settimeout(0.5)
        stop_event.clear()
        print("Please select an option:")
        print("1. Create room")
        print("2. Join room")
        print("3. List all rooms")
        print("4. List all online players")
        print("5. Invitation management")
        print("6. Game develop management")
        print("7. List all game")
        print("8. Logout")
        option = input("Your option: ")
        stop_event.set()
        time.sleep(0.5)
        sock.settimeout(original_timeout)
        if option == '1':
            Create(sock)
        elif option == '2':
            Join(sock)
        elif option == '3':
            List_room(sock)
        elif option == '4':
            List_player(sock)
        elif option == '5':
            Show_invite(sock)
        elif option == '6':
            Game_dev(sock)
        elif option == '7':
            Show_game(sock)
        elif option == '8':
            Logout(sock)
            # back to welcome
            login = False
        else:
            print("Option out of range!")

if __name__ == "__main__":
    main()
