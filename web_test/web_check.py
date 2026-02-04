#!/usr/bin/env python3
import subprocess, sys, time, os, random, select
import urllib.request
import http.client

IMAGE_MAGIC = {
	'png': 'image/png', 
	'jpg': 'image/jpeg', 
	'jpeg': 'image/jpeg', 
	'gif': 'image/gif', 
}

class Color:
    Blue = '\033[94m'
    Green = '\033[92m'
    Red = '\033[91m'
    Bold = '\033[1m'
    End = '\033[0m'


def do_query(url, check_header = True):
	try:
		conn = http.client.HTTPConnection("127.0.0.1", 8080)
		conn.request("GET", url)
		response = conn.getresponse()
	except Exception as ex:
		print('Error while reading HTTP server response: ', end='')
		print(ex)
		exit(1)
	try:
		data = response.read()
	except Exception as ex:
		print('Error while reading HTTP server response: ', end='')
		print(ex)
		exit(1)
	headers = dict((a, b) for a, b in response.getheaders())

	conn.close()

	if check_header:
		if not check('Content-Length' in headers, 'Content-Length in header'): 
			exit(1)
		if not check('Content-Type' in headers, 'Content-Type in header'): 
			exit(1)

	return response.status, data, headers

def check(expr, message):
	s = Color.Green + 'OK' if expr else Color.Red + 'FAIL'
	message += ':'
	print(f'  {message:<40} {Color.Bold}{s}{Color.End}')
	return expr

def try_root_url():
	status, data, headers = do_query('/')
	data = data.decode('utf8')

	if not check(os.path.isfile('index.html'), 'index.html exists'): 
		return False
	if not check(status == 200, 'Request to / returns code 200'): 
		return False

	with open('index.html') as rf:
		contents = rf.read()

	if not check(contents == data, 'Correct index.html contents'): 
		return False

	if not check(len(contents) == int(headers['Content-Length']), 'Content-Length matches file size'):
		return False

	if not check(headers['Content-Type'].lower() == 'text/html', 'Content-Type is text/html'):
		return False

	return True

def find_image(d):
	select_file = None
	ext = None
	for path in os.listdir(d):
		ext = path.split('.')[-1].strip().lower()
		if ext in ['jpg', 'png', 'gif', 'jpeg']:
			select_file = path
			break
	if d != '.':
		select_file = os.path.join(d, select_file)
	return select_file, ext

def try_image():

	select_file, ext = find_image('.')
	if select_file is None:
		select_file, ext = find_image('./img/')
	
	if not check(select_file is not None, 'Directory contains image files'): 
		print('Files in directory: ' + ', '.join(os.listdir()))
		return False

	with open(select_file, 'rb') as rf:
		contents = rf.read()

	status, data, headers = do_query(f'/{select_file}')
	if not check(contents == data, 'Correct image contents'): 
		return False

	if not check(len(contents) == int(headers['Content-Length']), 'Content-Length matches file size'):
		return False

	if not check(headers['Content-Type'].lower() == IMAGE_MAGIC[ext], f'Content-Type is {IMAGE_MAGIC[ext]}'):
		return False
	
	return True

def try_404():
	status, data, headers = do_query(f'/file_{random.randint(1000000, 10000000)}')
	data = data.decode('utf8')

	if not check(status == 404, 'Request to missing file returns 404'): 
		return False

	with open('index.html') as rf:
		contents = rf.read()

	if not check(contents != data, 'Error 404 has same data as index.html'): 
		return False

	return True

def try_multireq():
	with open('index.html') as rf:
		contents = rf.read()

	success = True
	for _ in range(20):
		status, data, headers = do_query('/', check_header = False)

		data = data.decode('utf8')
		if status != 200 or contents != data:
			success = False
			break

	if not check(success, '20 requests in fast succession'): 
		return False
	return True

def try_invalid_req():
	return True


tests = {
		'index.html': try_root_url, 
		'Images': try_image, 
		'404': try_404, 
		'Multiple requests': try_multireq
}

if len(sys.argv) < 2:
	print('Usage: ./web_check.py server_command')
	print('''
Note that all web server files have to be located in the same directory
as the web_check.py script. This includes the server executable, one
index.html file and at least one image file in jpeg, png or gif format. 
Also, the server has to run on port 8080. 

Examples:
./web_check.py ./server
./web_check.py ./a.out 8080
	   '''.strip())
	exit(1)

command = sys.argv[1:]

proc = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text = True)

time.sleep(0.5)
if proc.poll() is None:
	print(Color.Bold+"Process output: "+Color.End)
	ready, _, _ = select.select([proc.stdout, proc.stderr], [], [], 0)
	for stream in ready:
		if stream == process.stdout:
			print(stdout.readline().strip())
		elif stream == process.stderr:
			print(process.stderr.readline().strip())

for name, test in tests.items():
	print(f'{Color.Bold}Test {name}{Color.End}')
	if not test():
		exit(1)

proc.kill()
