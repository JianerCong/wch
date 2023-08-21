import web
import codecs

urls = (
    '/p2p/aaa', 'Aaa',
)

class Aaa:
    def GET(self):
        return 'aaa'

    def POST(self):
        data = web.data()
        # web.header('Content-Type', 'application/json')
        s = f'Data recieved: {codecs.decode(data)}'  # assume utf8
        print(s)
        return s

if __name__ == '__main__':
    app = web.application(urls, globals())
    app.run()
