# encoding: utf-8
# Sebastian Śledź © 2012
# sebasledz@gmail.com
import json
import sys

def usage():
	print 'usage: ' + appName + ' [--toArray | --toObject] [--flattenArray] [--humanReadable | --optimized] output_file.json input_1.json [... input_n.json]'
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
			if arg.lower() == '--toarray':
				outputToObject = False
			elif arg.lower() == '--toobject':
				outputToObject = True
			elif arg.lower() == '--flattenarray':
				flattenArray = True
			elif arg.lower() == '--humanreadable':
				humanReadable = True
			elif arg.lower() == '--optimized':
				humanReadable = False
			else:
				print 'Invalid option: ' + arg
				usage()
		else:
			break;
	else:
		index = index + 1

if len(argv) < 2:
	print 'To few arugments'
	usage()

inputFiles = argv[1:]
outputFile = argv[0]

if outputToObject:
	data = dict()
	for inputFile in inputFiles:
		inData = json.load(open(inputFile))
		if type(inData) is dict:
			data.update(inData)
		else:
			print 'Error: Can\'t append JSON data from \''+inputFile+'\' to dictionary - file doesn\'t contain dictionary.'
else:
	data = []
	for inputFile in inputFiles:
		inData = json.load(open(inputFile))
		if flattenArray and type(inData) is list:
			data = data + inData
		else:
			data.append(inData)

outFile = open(outputFile, 'w')
if humanReadable:
	ident = 4
else:
	ident = None
outFile.write(json.dumps(data, encoding = 'utf-8', ensure_ascii = False, indent = ident, sort_keys = True).encode('utf-8'))
