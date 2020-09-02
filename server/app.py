from __future__ import print_function

from flask import Flask, render_template, request, redirect, url_for
from oscpy.client import OSCClient

app = Flask(__name__, static_url_path='/static', static_folder='static/')
osc_client = OSCClient("127.0.0.1", 9000, encoding='utf8')


@app.route('/', methods = ['GET', 'POST'])
def index():
    if request.method == 'POST':
        query = request.form['query']
        osc_client.send_message(b'/new_query', [query])    
    
    return render_template("index.html")
    
if __name__ == '__main__':
    app.run(port=8123, debug=True)
