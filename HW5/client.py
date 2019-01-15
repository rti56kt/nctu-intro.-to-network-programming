import sys
import socket
import json
import os
import stomp
import time


class MyListener(stomp.ConnectionListener):
    def on_error(self, headers, message):
        print('received an error "%s"' % message)
    def on_message(self, headers, message):
        print(message)

def connectAMQ():
    conn = stomp.Connection([('35.172.32.15', 61613)])
    conn.set_listener('', MyListener())
    conn.start()
    conn.connect('admin', 'admin')
    return conn

class Client(object):
    def __init__(self, ip, port):
        try:
            socket.inet_aton(ip)
            if 0 < int(port) < 65535:
                self.ip = ip
                self.port = int(port)
            else:
                raise Exception('Port value should between 1~65535')
            self.cookie = {}
            self.sub_topic = {}
            self.login_ip = ip
            self.login_port = int(port)
            self.app_ip = {}
            self.app_port = {}
        except Exception as e:
            print(e, file=sys.stderr)
            sys.exit(1)

    def run(self):
        c = connectAMQ()
        while True:
            cmd = sys.stdin.readline()
            if cmd.rstrip() == 'exit':
                return
            if cmd != os.linesep:
                cmd = self.__attach_token(cmd)
                self.__which_server(cmd)
                while 1:
                    try:
                        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:

                            result = s.connect_ex((self.ip, self.port))
                            if result == 0:
                                s.send(cmd.encode())
                                # print(self.ip)
                                # print(cmd)
                                resp = s.recv(4096).decode()
                                # print(resp)
                                self.__show_result(json.loads(resp), cmd, c)
                                break
                            else:
                                time.sleep(10)
                    except Exception as e:
                        print(e, file=sys.stderr)
                        break

    def __show_result(self, resp, cmd=None, c=None):
        if 'message' in resp:
            print(resp['message'])

        if 'invite' in resp:
            if len(resp['invite']) > 0:
                for l in resp['invite']:
                    print(l)
            else:
                print('No invitations')

        if 'friend' in resp:
            if len(resp['friend']) > 0:
                for l in resp['friend']:
                    print(l)
            else:
                print('No friends')

        if 'post' in resp:
            if len(resp['post']) > 0:
                for p in resp['post']:
                    print('{}: {}'.format(p['id'], p['message']))
            else:
                print('No posts')

        if 'group' in resp:
            if len(resp['group']) > 0:
                for g in resp['group']:
                    print(g)
            else:
                print('No groups')

        if cmd:
            command = cmd.split()
            if resp['status'] == 0 and command[0] == 'login':
                self.cookie[command[1]] = resp['token']

                ####### Connect AMQ & Sub. #######
                if len(resp['subscribe']) > 0:
                    self.sub_topic[resp['token']] = resp['subscribe']
                    for s in resp['subscribe']:
                        t = '/topic/' + s
                        c.subscribe(t, resp['token']+s)
                q = '/topic/' + command[1]
                c.subscribe(q, resp['token'])


                ####### Get App Server IP #######
                if len(resp['appserverip']) > 0:
                    self.app_ip[resp['token']] = resp['appserverip']
                    self.app_port[resp['token']] = int(9000)



                
            if resp['status'] == 0 and command[0] == 'create-group':
                topicname = resp['subscribe']
                if command[1] in self.sub_topic:
                    self.sub_topic[command[1]].append(topicname)
                else:
                    self.sub_topic[command[1]] = [topicname]
                t = '/topic/' + topicname
                c.subscribe(t, command[1]+topicname)

            if resp['status'] == 0 and command[0] == 'join-group':
                topicname = resp['subscribe']
                if command[1] in self.sub_topic:
                    self.sub_topic[command[1]].append(topicname)
                else:
                    self.sub_topic[command[1]] = [topicname]
                t = '/topic/' + topicname
                c.subscribe(t, command[1]+topicname)

            if resp['status'] == 0 and (command[0] == 'logout' or command[0] == 'delete'):
                c.unsubscribe(command[1])
                if command[1] in self.sub_topic:
                    for topicname in self.sub_topic[command[1]]:
                        c.unsubscribe(command[1]+topicname)

                if (command[1] in self.app_ip) and (command[1] in self.app_port):
                        self.ip = self.login_ip
                        self.port = self.login_port

    def __which_server(self, cmd=None):
        if cmd:
            command = cmd.split()
            if len(command) > 1:
                if command[0] == 'register' or command[0] == 'login' or command[0] == 'logout' or command[0] == 'delete':
                    self.ip = self.login_ip
                    self.port = self.login_port
                else:
                    if (command[1] in self.app_ip) and (command[1] in self.app_port):
                        self.ip = self.app_ip[command[1]]
                        self.port = self.app_port[command[1]]
                    else:
                        self.ip = self.login_ip
                        self.port = self.login_port
            else:
                self.ip = self.login_ip
                self.port = self.login_port
        else:
            pass


    def __attach_token(self, cmd=None):
        if cmd:
            command = cmd.split()
            if len(command) > 1:
                if command[0] != 'register' and command[0] != 'login':
                    if command[1] in self.cookie:
                        command[1] = self.cookie[command[1]]
                    else:
                        command.pop(1)
            return ' '.join(command)
        else:
            return cmd


def launch_client(ip, port):
    c = Client(ip, port)
    c.run()

if __name__ == '__main__':
    if len(sys.argv) == 3:
        launch_client(sys.argv[1], sys.argv[2])
    else:
        print('Usage: python3 {} IP PORT'.format(sys.argv[0]))
