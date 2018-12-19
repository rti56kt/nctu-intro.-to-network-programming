import peewee as pw

db = pw.SqliteDatabase('database.db', pragmas={'foreign_keys': 1})


class BaseModel(pw.Model):
    class Meta:
        database = db


class User(BaseModel):
    username = pw.CharField(unique=True)
    password = pw.CharField()


class Invitation(BaseModel):
    inviter = pw.ForeignKeyField(User, on_delete='CASCADE')
    invitee = pw.ForeignKeyField(User, on_delete='CASCADE')


class Friend(BaseModel):
    user = pw.ForeignKeyField(User, on_delete='CASCADE')
    friend = pw.ForeignKeyField(User, on_delete='CASCADE')


class Post(BaseModel):
    user = pw.ForeignKeyField(User, on_delete='CASCADE')
    message = pw.CharField()


class Follow(BaseModel):
    follower = pw.ForeignKeyField(User, on_delete='CASCADE')
    followee = pw.ForeignKeyField(User, on_delete='CASCADE')


class Token(BaseModel):
    token = pw.CharField(unique=True)
    owner = pw.ForeignKeyField(User, on_delete='CASCADE')


class Topic(BaseModel):
    topicname = pw.CharField(unique=True)


class Subscribe(BaseModel):
    user = pw.ForeignKeyField(User, on_delete='CASCADE')
    topic = pw.ForeignKeyField(Topic, on_delete='CASCADE')

if __name__ == '__main__':
    db.connect()
    db.create_tables([User, Invitation, Friend, Post, Follow, Token, Topic, Subscribe])
