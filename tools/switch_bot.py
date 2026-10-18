"""Remote control of the Switch through usb-botbase in Wi-Fi mode (docs/switch.md "Remote testing").

usb-botbase (github.com/Koi-3088/usb-botbase, a fork of sys-botbase) is an Atmosphère sysmodule
(sdmc:/atmosphere/contents/430000000000000B, config.cfg = "wifi") listening on TCP 6000 for text commands ending in
\\r\\n: button presses, screenshots (pixelPeek, a JPEG as hex), the running title. Wi-Fi mode needs no PC driver and
leaves the USB port to DBI's MTP responder (tools/switch_mtp.py).

usage: python -I tools/switch_bot.py find                      scan the PC's /24 for port 6000, remember the address
       python -I tools/switch_bot.py cmd <command...>          send one command, print the answer
       python -I tools/switch_bot.py shot <out.jpg>            screenshot
       python -I tools/switch_bot.py click <button...>         e.g. click HOME A (0.6 s apart)
       python -I tools/switch_bot.py title                     title id of the running program (0 = none)
Address: --ip <a.b.c.d> or SWITCH_IP, else the one `find` stored in port/switch/bot_ip.txt.
"""
import concurrent.futures
import os
import socket
import sys
import time

PORT = 6000
HERE = os.path.dirname(os.path.abspath(__file__))
IP_FILE = os.path.join(HERE, '..', 'port', 'switch', 'bot_ip.txt')
# answers with a line; the other commands (click, press, ...) answer nothing
ANSWERING = {'pixelPeek', 'getTitleID', 'getVersion', 'getMainNsoBase', 'getHeapBase', 'getBuildID',
             'getTitleVersion', 'getSystemLanguage', 'isProgramRunning', 'charge', 'game', 'peek', 'peekMain',
             'peekAbsolute'}


def local_prefix():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(('192.0.2.1', 9))  # no packet is sent: picks the interface of the default route
        return s.getsockname()[0].rsplit('.', 1)[0]
    finally:
        s.close()


def probe(ip):
    try:
        with socket.create_connection((ip, PORT), timeout=0.6) as s:
            s.settimeout(3)
            s.sendall(b'getVersion\r\n')
            answer = s.recv(64).decode('ascii', 'replace').strip()
            return ip if answer else None
    except OSError:
        return None


def find():
    prefix = local_prefix()
    with concurrent.futures.ThreadPoolExecutor(64) as pool:
        hits = [ip for ip in pool.map(probe, [f'{prefix}.{i}' for i in range(1, 255)]) if ip]
    if not hits:
        sys.exit(f'no usb-botbase on {prefix}.0/24 port {PORT} (Switch on, same network, rebooted after install?)')
    os.makedirs(os.path.dirname(IP_FILE), exist_ok=True)
    open(IP_FILE, 'w').write(hits[0])
    return hits[0]


class Bot:
    def __init__(self, ip):
        self.sock = socket.create_connection((ip, PORT), timeout=10)
        self.file = self.sock.makefile('rb')

    def send(self, command):
        self.sock.sendall(command.encode('ascii') + b'\r\n')
        if command.split()[0] in ANSWERING:
            return self.file.readline().decode('ascii', 'replace').strip()
        return None

    def shot(self, path):
        data = bytes.fromhex(self.send('pixelPeek'))
        if not data.startswith(b'\xff\xd8'):
            sys.exit('pixelPeek returned no JPEG (capture refused by the system?)')
        open(path, 'wb').write(data)
        return len(data)

    def click(self, *buttons, gap=0.6):
        for b in buttons:
            self.send(f'click {b}')
            time.sleep(gap)


def address(argv):
    if '--ip' in argv:
        i = argv.index('--ip')
        ip = argv[i + 1]
        del argv[i:i + 2]
        return ip
    if os.environ.get('SWITCH_IP'):
        return os.environ['SWITCH_IP']
    if os.path.exists(IP_FILE):
        return open(IP_FILE).read().strip()
    return find()


def main(argv):
    if len(argv) < 2:
        sys.exit(__doc__)
    if argv[1] == 'find':
        print(find())
        return
    bot = Bot(address(argv))
    if argv[1] == 'cmd':
        answer = bot.send(' '.join(argv[2:]))
        if answer is not None:
            print(answer)
    elif argv[1] == 'shot':
        print(f'{bot.shot(argv[2])} bytes -> {argv[2]}')
    elif argv[1] == 'click':
        bot.click(*argv[2:])
    elif argv[1] == 'title':
        print(bot.send('getTitleID'))
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main(sys.argv)
