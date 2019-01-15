import sys
import socket
from peewee import * # pylint: disable=W0614
from model import * # pylint: disable=W0614
import boto3
import json
import uuid
import stomp


def connectAMQ():
    conn = stomp.Connection([('35.172.32.15', 61613)])
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





            ####### Distribute server to client #######    
            res = Servermap.select().group_by(Servermap.serverdistributed).having(fn.COUNT(Servermap.serverdistributed)<10).first()
            if res:
                Servermap.create(user=t.owner, serverdistributed=res.serverdistributed)
                distributed_ip = res.serverdistributed.serverip
            else:
                ec2 = boto3.client('ec2')

                user_data =  """#!/bin/bash
                python3 /home/ubuntu/server.py 0.0.0.0 9000 >> /home/ubuntu/run_server.log 2>&1
                exit 0"""

                result = ec2.run_instances(
                    ImageId='ami-0abb656182ac3630f', 
                    InstanceType='t2.micro', 
                    MinCount=1, 
                    MaxCount=1, 
                    SecurityGroups=['launch-wizard-2'], 
                    KeyName='MyKeyPair', 
                    UserData=user_data
                )

                for instance in result['Instances']:
                    instance_id = instance['InstanceId']

                ec2_r = boto3.resource('ec2')
                newserver = ec2_r.Instance(id=instance_id)
                newserver.wait_until_running()

                distributed_ip = ec2.describe_instances(InstanceIds=[instance_id])['Reservations'][0]['Instances'][0]['PublicIpAddress']
                
                serverid = Serverpool.create(instanceid=instance_id, serverip=distributed_ip)
                Servermap.create(user=t.owner, serverdistributed=serverid)


            return {
                'status': 0,
                'token': t.token,
                'message': 'Success!',
                'subscribe': topics,
                'appserverip': distributed_ip
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

        res = (Serverpool
        .select()
        .join(Servermap, JOIN.LEFT_OUTER, on=(Serverpool.id==Servermap.serverdistributed))
        .where(Servermap.serverdistributed.is_null())
        )
        if res:
            for r in res:
                ec2 = boto3.client('ec2')
                ec2.terminate_instances(InstanceIds=[r.instanceid])
                r.delete_instance()
                # print(r.instanceid)
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
        Servermap.delete().where(Servermap.user == token.owner).execute()
        token.delete_instance()

        res = (Serverpool
        .select()
        .join(Servermap, JOIN.LEFT_OUTER, on=(Serverpool.id==Servermap.serverdistributed))
        .where(Servermap.serverdistributed.is_null())
        )
        if res:
            for r in res:
                ec2 = boto3.client('ec2')
                ec2.terminate_instances(InstanceIds=[r.instanceid])
                r.delete_instance()
        return {
            'status': 0,
            'message': 'Bye!'
        }


    @__auth
    def invite(self, token, username=None, *args):
        pass

    @__auth
    def list_invite(self, token, *args):
        pass

    @__auth
    def accept_invite(self, token, username=None, *args):
        pass

    @__auth
    def list_friend(self, token, *args):
        pass

    @__auth
    def post(self, token, *args):
        pass

    @__auth
    def receive_post(self, token, *args):
        pass

    @__auth
    def send(self, token, username=None, *args):
        pass
    
    @__auth
    def create_group(self, token, topicname=None, *args):
        pass

    @__auth
    def list_group(self, token, *args):
        pass

    @__auth
    def list_joined(self, token, *args):
        pass

    @__auth
    def join_group(self, token, topicname=None, *args):
        pass

    @__auth
    def send_group(self, token, topicname=None, *args):
        pass

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
            Servermap.delete().execute()
            Serverpool.delete().execute()
        except Exception as e:
            print(e, file=sys.stderr)
            sys.exit(1)

    def run(self):
        self.sock.bind((self.ip, self.port))
        self.sock.listen(100)
        socket.setdefaulttimeout(0.1)
        while True:
            try:
                conn, _addr = self.sock.accept()
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
