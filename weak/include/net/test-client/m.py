import web
import json

urls = (
    '/', 'hi',
    '/json', 'serveJSON',
)

class hi:
    def GET(self):
        return 'aaa'

class serveJSON:
    def GET(self):
        d = {'one':1,'two':2}
        web.header('Content-Type', 'application/json')
        return json.dumps(d)

    def POST(self):
        data = web.data()
        web.header('Content-Type', 'application/json')
        print(f'Data recieved: {json.loads(data)}')
        return json.dumps({'msg':'Okay'})


if __name__ == '__main__':
    app = web.application(urls, globals())
    app.run()
