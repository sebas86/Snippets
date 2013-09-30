# encoding: utf-8
# Sebastian Śledź © 2012-2013
# sebasledz@gmail.com
import json
import sys

def usage():
	print('usage: ' + appName + ' [--toArray | --toObject] [--flattenArray] [--humanReadable | --optimized] output_file.json input_1.json [... input_n.json]')
	sys.exit(-1)

argv = sys.argv[1:]
appName = sys.argv[0]

outputToObject = True
flattenArray = False
humanReadable = False
index = 0
while index < len(argv):
	arg = argv[index]
	if arg[0:2] == '--':
		argv.pop(index)
		if len(arg) > 2:
			argLowCase = arg.lower()
			if argLowCase == '--toarray':
				outputToObject = False
			elif argLowCase == '--toobject':
				outputToObject = True
			elif argLowCase == '--flattenarray':
				flattenArray = True
			elif argLowCase == '--humanreadable':
				humanReadable = True
			elif argLowCase == '--optimized':
				humanReadable = False
			else:
				print('Invalid option: ' + arg)
				usage()
		else:
			break;
	else:
		index = index + 1

if len(argv) < 2:
	print('To few arugments')
	usage()

inputFiles = argv[1:]
outputFile = argv[0]

if outputToObject:
	data = {}
	for inputFile in inputFiles:
		inData = json.load(open(inputFile))
		if type(inData) is dict:
			data.update(inData)
		else:
			print('Error: Can\'t append JSON data from \''+inputFile+'\' to dictionary - file doesn\'t contain dictionary.')
else:
	data = []
	for inputFile in inputFiles:
		inData = json.load(open(inputFile))
		if flattenArray and type(inData) is list:
			data = data + inData
		else:
			data.append(inData)

if humanReadable:
	ident = 4
else:
	ident = None

outFile = open(outputFile, 'w')
if int(sys.version[0]) < 3:
    outFile.write(json.dumps(data, encoding = 'utf-8', ensure_ascii = False, indent = ident, sort_keys = True).encode('utf-8'))
else:
    outFile.write(json.dumps(data, ensure_ascii = False, indent = ident, sort_keys = True))
