import sys
import socket
from model import *
import json
import uuid
import stomp


def connectAMQ():
    conn = stomp.Connection([('localhost', 61613)])
    conn.start()
    conn.connect('admin', 'admin')
    return conn

class DBControl(object):
    def __auth(func):
        def validate_token(self, token=None, *args):
            if token:
                t = Token.get_or_none(Token.token == token)
                if t:
                    return func(self, t, *args)
            return {
                'status': 1,
                'message': 'Not login yet'
            }
        return validate_token

    def register(self, username=None, password=None, *args):
        if not username or not password or args:
            return {
                'status': 1,
                'message': 'Usage: register <username> <password>'
            }
        if User.get_or_none(User.username == username):
            return {
                'status': 1,
                'message': '{} is already used'.format(username)
            }
        res = User.create(username=username, password=password)
        if res:
            return {
                'status': 0,
                'message': 'Success!'
            }
        else:
            return {
                'status': 1,
                'message': 'Register failed due to unknown reason'
            }

    def login(self, username=None, password=None, *args):
        if not username or not password or args:
            return {
                'status': 1,
                'message': 'Usage: login <id> <password>'
            }
        res = User.get_or_none((User.username == username) & (User.password == password))
        if res:
            t = Token.get_or_none(Token.owner == res)
            if not t:
                t = Token.create(token=str(uuid.uuid4()), owner=res)

            ####### Find Topics That User Sub. #######
            res = Subscribe.select().where(Subscribe.user == t.owner)
            topics = []
            for r in res:
                topics.append(r.topic.topicname)

            return {
                'status': 0,
                'token': t.token,
                'message': 'Success!',
                'subscribe': topics
            }
        else:
            return {
                'status': 1,
                'message': 'No such user or password error'
            }


    @__auth
    def delete(self, token, *args):
        if args:
            return {
                'status': 1,
                'message': 'Usage: delete <user>'
            }
        token.owner.delete_instance()
        return {
            'status': 0,
            'message': 'Success!'
        }

    @__auth
    def logout(self, token, *args):
        if args:
            return {
                'status': 1,
                'message': 'Usage: logout <user>'
            }
        token.delete_instance()
        return {
            'status': 0,
            'message': 'Bye!'
        }

    @__auth
    def invite(self, token, username=None, *args):
        if not username or args:
            return {
                'status': 1,
                'message': 'Usage: invite <user> <id>'
            }
        if username == token.owner.username:
            return {
                'status': 1,
                'message': 'You cannot invite yourself'
            }
        friend = User.get_or_none(User.username == username)
        if friend:
            res1 = Friend.get_or_none((Friend.user == token.owner) & (Friend.friend == friend))
            res2 = Friend.get_or_none((Friend.friend == token.owner) & (Friend.user == friend))
            if res1 or res2:
                return {
                    'status': 1,
                    'message': '{} is already your friend'.format(username)
                }
            else:
                invite1 = Invitation.get_or_none((Invitation.inviter == token.owner) & (Invitation.invitee == friend))
                invite2 = Invitation.get_or_none((Invitation.inviter == friend) & (Invitation.invitee == token.owner))
                if invite1:
                    return {
                        'status': 1,
                        'message': 'Already invited'
                    }
                elif invite2:
                    return {
                        'status': 1,
                        'message': '{} has invited you'.format(username)
                    }
                else:
                    Invitation.create(inviter=token.owner, invitee=friend)
                    return {
                        'status': 0,
                        'message': 'Success!'
                    }
        else:
            return {
                'status': 1,
                'message': '{} does not exist'.format(username)
            }
        pass

    @__auth
    def list_invite(self, token, *args):
        if args:
            return {
                'status': 1,
                'message': 'Usage: list-invite <user>'
            }
        res = Invitation.select().where(Invitation.invitee == token.owner)
        invite = []
        for r in res:
            invite.append(r.inviter.username)
        return {
            'status': 0,
            'invite': invite
        }

    @__auth
    def accept_invite(self, token, username=None, *args):
        if not username or args:
            return {
                'status': 1,
                'message': 'Usage: accept-invite <user> <id>'
            }
        inviter = User.get_or_none(User.username == username)
        invite = Invitation.get_or_none((Invitation.inviter == inviter) & (Invitation.invitee == token.owner))
        if invite:
            Friend.create(user=token.owner, friend=inviter)
            invite.delete_instance()
            return {
                'status': 0,
                'message': 'Success!'
            }
        else:
            return {
                'status': 1,
                'message': '{} did not invite you'.format(username)
            }
        pass

    @__auth
    def list_friend(self, token, *args):
        if args:
            return {
                'status': 1,
                'message': 'Usage: list-friend <user>'
            }
        friends = Friend.select().where((Friend.user == token.owner) | (Friend.friend == token.owner))
        res = []
        for f in friends:
            if f.user == token.owner:
                res.append(f.friend.username)
            else:
                res.append(f.user.username)
        return {
            'status': 0,
            'friend': res
        }

    @__auth
    def post(self, token, *args):
        if len(args) <= 0:
            return {
                'status': 1,
                'message': 'Usage: post <user> <message>'
            }
        Post.create(user=token.owner, message=' '.join(args))
        return {
            'status': 0,
            'message': 'Success!'
        }

    @__auth
    def receive_post(self, token, *args):
        if args:
            return {
                'status': 1,
                'message': 'Usage: receive-post <user>'
            }
        res = Post.select().where(Post.user != token.owner).join(Friend, on=((Post.user == Friend.user) | (Post.user == Friend.friend))).where((Friend.user == token.owner) | (Friend.friend == token.owner))
        post = []
        for r in res:
            post.append({
                'id': r.user.username,
                'message': r.message
            })
        return {
            'status': 0,
            'post': post
        }

    @__auth
    def send(self, token, username=None, *args):
        if not username or len(args) <= 0:
            return {
                'status': 1,
                'message': 'Usage: send <user> <friend> <message>'
            }
        if username == token.owner.username:
            return {
                'status': 1,
                'message': 'You cannot send message to yourself'
            }
        friend = User.get_or_none(User.username == username)
        if friend:
            res1 = Friend.get_or_none((Friend.user == token.owner) & (Friend.friend == friend))
            res2 = Friend.get_or_none((Friend.friend == token.owner) & (Friend.user == friend))
            if res1 or res2:
                if_online = Token.get_or_none(Token.owner == friend)
                if if_online:
                    sender = token.owner.username
                    receiver = username
                    send_message = '<<<' + sender + '->' + receiver + ': ' + ' '.join(args) + '>>>'
                    c = connectAMQ()
                    q = '/topic/' + receiver
                    c.send(q, send_message)
                    c.disconnect()
                    return {
                        'status': 0,
                        'message': 'Success!'
                    }
                else:
                    return {
                        'status': 1,
                        'message': '{} is not online'.format(username)
                    }
            else:
                return {
                'status': 1,
                'message': '{} is not your friend'.format(username)
            }
        else:
            return {
                'status': 1,
                'message': 'No such user exist'
            }
    
    @__auth
    def create_group(self, token, topicname=None, *args):
        if not topicname or args:
            return {
                'status': 1,
                'message': 'Usage: create-group <user> <group>'
            }
        group = Topic.get_or_none(Topic.topicname == topicname)
        if group:
            return {
                'status': 1,
                'message': '{} already exist'.format(topicname)
            }
        else:
            res1 = Topic.create(topicname=topicname)
            group = Topic.get_or_none(Topic.topicname == topicname)
            res2 = Subscribe.create(user = token.owner, topic = group)
            if res1 and res2:
                return {
                    'status': 0,
                    'message': 'Success!',
                    'subscribe': topicname
                }
            else:
                return {
                    'status': 1,
                    'message': 'create_group failed due to unknown reason'
                }

    @__auth
    def list_group(self, token, *args):
        if args:
            return {
                'status': 1,
                'message': 'Usage: list-group <user>'
            }
        res = Topic.select()
        groups = []
        for r in res:
            groups.append(r.topicname)
        return {
            'status': 0,
            'group': groups
        }

    @__auth
    def list_joined(self, token, *args):
        if args:
            return {
                'status': 1,
                'message': 'Usage: list-join <user>'
            }
        res = Subscribe.select().where(Subscribe.user == token.owner)
        joins = []
        for r in res:
            joins.append(r.topic.topicname)
        return {
            'status': 0,
            'group': joins
        }

    @__auth
    def join_group(self, token, topicname=None, *args):
        if not topicname or args:
            return {
                'status': 1,
                'message': 'Usage: join-group <user> <group>'
            }
        group = Topic.get_or_none(Topic.topicname == topicname)
        if group:
            res = Subscribe.get_or_none((Subscribe.user == token.owner) & (Subscribe.topic == group))
            if res:
                return {
                    'status': 1,
                    'message': 'Already a member of {}'.format(topicname)
                }
            else:
                Subscribe.create(user = token.owner, topic = group)
                return {
                    'status': 0,
                    'message': 'Success!',
                    'subscribe': topicname
                }
        else:
            return {
                'status': 1,
                'message': '{} does not exist'.format(topicname)
            }

    @__auth
    def send_group(self, token, topicname=None, *args):
        if not topicname or len(args) <= 0:
            return {
                'status': 1,
                'message': 'Usage: send-group <user> <group> <message>'
            }
        group = Topic.get_or_none(Topic.topicname == topicname)
        if group:
            res = Subscribe.get_or_none((Subscribe.user == token.owner) & (Subscribe.topic == group))
            if res:
                sender = token.owner.username
                receiver = topicname
                send_message = '<<<' + sender + '->GROUP<' + receiver + '>: ' + ' '.join(args) + '>>>'
                c = connectAMQ()
                t = '/topic/' + receiver
                c.send(t, send_message)
                c.disconnect()
                return {
                    'status': 0,
                    'message': 'Success!'
                }
            else:
                return {
                    'status': 1,
                    'message': 'You are not the member of {}'.format(topicname)
                }
        else:
            return {
                'status': 1,
                'message': 'No such group exist'
            }


class Server(object):
    def __init__(self, ip, port):
        try:
            socket.inet_aton(ip)
            if 0 < int(port) < 65535:
                self.ip = ip
                self.port = int(port)
            else:
                raise Exception('Port value should between 1~65535')
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.db = DBControl()
            Token.delete().execute()
        except Exception as e:
            print(e, file=sys.stderr)
            sys.exit(1)

    def run(self):
        self.sock.bind((self.ip, self.port))
        self.sock.listen(100)
        socket.setdefaulttimeout(0.1)
        while True:
            try:
                conn, addr = self.sock.accept()
                with conn:
                    cmd = conn.recv(4096).decode()
                    resp = self.__process_command(cmd)
                    conn.send(resp.encode())
            except Exception as e:
                print(e, file=sys.stderr)

    def __process_command(self, cmd):
        command = cmd.split()
        if len(command) > 0:
            command_exec = getattr(self.db, command[0].replace('-', '_'), None)
            if command_exec:
                return json.dumps(command_exec(*command[1:]))
        return self.__command_not_found(command[0])

    def __command_not_found(self, cmd):
        return json.dumps({
            'status': 1,
            'message': 'Unknown command {}'.format(cmd)
        })


def launch_server(ip, port):
    c = Server(ip, port)
    c.run()

if __name__ == '__main__':
    if sys.argv[1] and sys.argv[2]:
        launch_server(sys.argv[1], sys.argv[2])
