# TODO:
# Passkey and config files
# include wifi setup tool/admin panel (when the ESP can't connect to wifi after timeout, it should open a wifi network where you can access it and provide new WIFI credentials)
#
# Note: please don't judge me, I didn't really have much time to put into this project :D
#

from pynput import keyboard
import requests
import threading
import time
import subprocess
import re
import socket

ESP_MAC_ADDRESS = "mac".lower()
MAX_ERRORS = 3
ERROR_COUNTER = MAX_ERRORS

# ESP8266 server address
# this will be updated dynamically if it can't be resolved
DEFAULT_ESP_SERVER = "standingdesk.local"
ESP_SERVER = None

# track the state of key combinations
alt_page_up_active = False
alt_page_down_active = False
pressed_keys = set()

def find_esp_ip():
    global ESP_SERVER

    print("[INFO] Searching for ESP IP address...")
    while not ESP_SERVER:
        try:
            # try the default server address first
            print(f"[INFO] Trying `{DEFAULT_ESP_SERVER}`... ")
            res = requests.get(f"http://{DEFAULT_ESP_SERVER}")
            if res.status_code == 200:
                # mDNS is working
                # get the IP address to skip DNS lookup delays
                ip_address = socket.gethostbyname(DEFAULT_ESP_SERVER)
                ESP_SERVER = f"http://{ip_address}"
                print(f"[INFO] Found ESP at {ESP_SERVER}")
                return

            print("[INFO] Not working. Looking up ARP table.. ")
            # run the ARP command and parse the output
            arp_output = subprocess.check_output("arp -a", shell=True).decode()
            for line in arp_output.splitlines():
                if ESP_MAC_ADDRESS in line.lower():
                    match = re.search(r"(\d+\.\d+\.\d+\.\d+)", line)
                    if match:
                        ESP_SERVER = f"http://{match.group(1)}"
                        print(f"[INFO] Found ESP at {ESP_SERVER}")
                        return
            break
        except Exception as e:
            print(f"[ERROR] Failed to find ESP IP: {e}")
        time.sleep(2)

def send_command(endpoint):
    global ERROR_COUNTER, ESP_SERVER
    try:
        response = requests.get(f"{ESP_SERVER}/{endpoint}", timeout=5)
        print(f"[DEBUG] Sent {endpoint}: {response.text}")
    except requests.exceptions.RequestException as e:
        print(f"[ERROR] Failed to send {endpoint}: {e}")
        ERROR_COUNTER -= 1

        if ERROR_COUNTER == 0:
            ESP_SERVER = None
            find_esp_ip()
            ERROR_COUNTER = MAX_ERRORS

# handle key press events
def on_press(key):
    global alt_page_up_active, alt_page_down_active
    try:
        pressed_keys.add(key)
        if key == keyboard.Key.page_up:
            if keyboard.Key.alt_l in pressed_keys or keyboard.Key.alt_r in pressed_keys:
                if not alt_page_up_active:
                    alt_page_up_active = True
                    print("[DEBUG] Alt + Page Up pressed. Sending 'up' command.")
                    threading.Thread(target=send_command, args=("up",)).start()

            elif keyboard.Key.cmd in pressed_keys:
                print("[DEBUG] Win + Page Up pressed. Sending 'standing' command.")
                threading.Thread(target=send_command, args=("standing",)).start()

        elif key == keyboard.Key.page_down:
            if keyboard.Key.alt_l in pressed_keys or keyboard.Key.alt_r in pressed_keys:
                if not alt_page_down_active:
                    alt_page_down_active = True
                    print("[DEBUG] Alt + Page Down pressed. Sending 'down' command.")
                    threading.Thread(target=send_command, args=("down",)).start()

            elif keyboard.Key.cmd in pressed_keys:
                print("[DEBUG] Win + Page Down pressed. Sending 'seating' command.")
                threading.Thread(target=send_command, args=("seating",)).start()

    except AttributeError as e:
        print(f"[ERROR] Key press error: {e}")

# handle key release events
def on_release(key):
    global alt_page_up_active, alt_page_down_active

    try:
        pressed_keys.remove(key)
    except Exception as e:
        pass

    try:
        if key == keyboard.Key.page_up and alt_page_up_active:
            alt_page_up_active = False
            print("[DEBUG] Alt + Page Up released. Sending 'stop' command.")
            threading.Thread(target=send_command, args=("stop",)).start()

        elif key == keyboard.Key.page_down and alt_page_down_active:
            alt_page_down_active = False
            print("[DEBUG] Alt + Page Down released. Sending 'stop' command.")
            threading.Thread(target=send_command, args=("stop",)).start()

    except AttributeError as e:
        print(f"[ERROR] Key release error: {e}")

# listen for key events
def listen_keyboard():
    with keyboard.Listener(on_press=on_press, on_release=on_release) as listener:
        listener.join()

if __name__ == "__main__":
    find_esp_ip()
    print("Listening for keyboard shortcuts...")

    try:
        keyboard_listener = threading.Thread(target=listen_keyboard)
        keyboard_listener.start()
        keyboard_listener.join()
    except KeyboardInterrupt:
        print("[INFO] Exiting program.")
