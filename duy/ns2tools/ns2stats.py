#!/usr/bin/python

"""A python module for starting NS2 and collecting statistics from it. The
units for values are as follows:

throughput - Bytes/second
drops - Ratio of drops/totalPackets
delay - Seconds

If this script is executed directly, it parses the trace file passed as the
first argument and prints the results. It only supports new style traces.
"""

import re
import os
import os.path
import sys
import tempfile
import errno

import signal

import frange

POSITIVE_INFINITY = 1e300000

def parseAggregateStatistics( lines, trimFromFront = 5, trimFromBack = 5 ):
	"""parseThroughputStatistics( iterable, number, number ) -> ( throughput, drops, averageDelay )
	
	Parses the specified set of ns2 lines (in new trace format) and returns
	the aggregate throughput, ratio of dropped packets, and the average
	delay. For TCP streams, it returns the goodput, not throughput. The
	statistics returned are averaged across the entire set of streams.
	
	lines - An iterable object containing lines to be parsed.
	trimFromFront - Number of seconds of warm up to ignore.
	trimFromBack - Number of seconds of cool down to ignore."""
	
	PARSE_TRACE = re.compile( '-([A-z]+) ([^ ]+)' )
	
	NS2_TIME = 't'
	NS2_AGENT = 'Nl'
	NS2_PACKET_TYPE = 'It'
	NS2_PROTOCOL = 'P'
	NS2_SOURCE = 'Is'
	NS2_DESTINATION = 'Id'
	NS2_PACKET_ID = 'Ii'
	NS2_PACKET_LENGTH = 'Il'
	NS2_TCP_SEQUENCE = 'Ps'

	timeStamp = None
	timeStampIndexes = None
	
	# Maps a packet key (sender, destination, packet) to a tuple:
	# (type, sent time, receive/drop time, packet length)
	packetMap = {}
	# Maps packet ids to sent time
	packetPendingMap = {}
	
	# Used to ignore duplicates in TCP streams
	receivedSequenceNumbers = {}
	numDuplicates = 0
	
	bytes = 0
	drops = 0
	
	for line in lines:
		# If the line is not a send, receive or a drop, ignore it
		type = line[0]
		if type != 's' and type != 'r' and type != 'd':
			continue
		
		start = line.index( '-Nl', 3 ) + 4
		agentType = line[start:start+3]
		# We only care about send and recieve events on agents,
		# but drop events happen at intermediate routers
		if agentType != "AGT" and agentType != "IFQ" and not (type == 'd' and agentType == 'RTR'):
			continue
		
		# Parse the line
		parsed = dict( PARSE_TRACE.findall( line ) )
		
		# Locate the type		
		try:
			packetType = parsed[NS2_PACKET_TYPE]
		except KeyError:
			# Gross hack to ignore ARP packets which have no IP information
			assert parsed[NS2_PROTOCOL] == 'arp'
			continue
		
		# Ignore TCP acknowledgements
		if packetType == "ack":
			continue
		
		# Locate the source
		src = float( parsed[NS2_SOURCE] )
		
		# Locate the destination
		dst = float( parsed[NS2_DESTINATION] )
			
		# Locate the packet id: this is not unique when there are multiple flows
		id = int( parsed[NS2_PACKET_ID] )
		
		key = ( src, dst, id )
		
		timeStamp = float( parsed[NS2_TIME] )
		
		if type == 's':
			assert( agentType == "AGT" )
			packetPendingMap[key] = timeStamp
		else:
			assert( type == 'd' or type == 'r' )
			# The packet should be pending
			try:
				sentTimeStamp = packetPendingMap[key]
				del packetPendingMap[key]
			except KeyError:
				# It is possible that we had a "spurious drop" or a duplicate receive
				# Wireless networks are fun that way. Ignore spurious drops.
				if type == 'd':
					continue
				assert( type == 'r' )
				
				# if we already received this once, then increment numDuplicates
				previous = packetMap[key]
				if previous[0] == 'r':
					numDuplicates += 1
					continue
				
				# We previously thought this packet was dropped. We will update it to be recieved
				assert( previous[0] == 'd' )
				sentTimeStamp = previous[1]
			
			packetLength = 0
			if type == 'r':
				# Locate packet length
				packetLength = int( parsed[NS2_PACKET_LENGTH] )
					
				# Locate the TCP sequence number and ignore duplicates
				if packetType == "tcp":
					seqNum = int( parsed[NS2_TCP_SEQUENCE] )
					
					# The keys are (src, dst, sequence #), to avoid "false duplicates" caused
					# by multiple TCP streams.
					tcpkey = ( src, dst, seqNum )
					
					if tcpkey in receivedSequenceNumbers:
						# This is a duplicate reception: ignore it
						#~ print "duplicate:",line
						numDuplicates += 1
						continue
					else:
						receivedSequenceNumbers[tcpkey] = 1
			
			# move the event to the packet map
			packetMap[key] = (type, sentTimeStamp, timeStamp, packetLength)

	if len( packetMap ) == 0:
		return ( 0, 0, 0, {} )

	# Figure out the final time by rounding the last event time and subtracting the trim time
	finalTime = int(timeStamp - trimFromBack + 0.5) 
	
	flows = {}
	
	totalDelay = 0
	totalCountedPackets = 0
	for key, packet in packetMap.iteritems():
		# The flow ID is the source, destination
		# Make sure there is a flow entry, even if we did not collect any packets for it
		# due to the time range restrictions
		flowId = ( key[0], key[1] )
		if flowId not in flows:
			flows[flowId] = [ 0, 0, 0, 0 ]
		
		# If this packet is outside the designated time range, ignore it
		if packet[2] < trimFromFront:
			continue
		if packet[2] > finalTime:
			continue
		
		totalCountedPackets += 1
		
		# Increment the "total packets" count
		flows[flowId][3] += 1
			
		if packet[0] == 'r':
			delay = packet[2] - packet[1]
			assert( delay > 0 )
			totalDelay += delay
			bytes += packet[3]
			
			flows[flowId][0] += packet[3]
			flows[flowId][2] += delay
		else:
			assert( packet[0] == 'd' )
			drops += 1
			
			flows[flowId][1] += 1
	
	throughput = float( bytes ) / float( finalTime - trimFromFront )
	averageDelay = float( totalDelay ) / (totalCountedPackets - drops)
	drops = float( drops ) / totalCountedPackets
	
	for flowId, values in flows.iteritems():
		bytes, d, totalDelay, packets = values
		
		if packets == 0:
			flows[flowId] = ( 0, 0, 0 )
		else:
			tput = float( bytes ) / float( finalTime - trimFromFront )
			
			packetsReceived = packets - d
			delay = POSITIVE_INFINITY
			if packetsReceived != 0:
				delay = float( totalDelay ) / packetsReceived
			
			d = float( d ) / packets
			
			flows[flowId] = ( tput, d, delay )
	
	return ( throughput, drops, averageDelay, flows )

def sigchldHandler( signalNumber, stackFrame ):
	return

def sigalarmHandler( signalNumber, stackFrame ):
	return

def executeNs2( ns2Command ):
	"""Executes the specified ns2 command and passes a file object that
	accesses the trace file output. This function creates a FIFO, then forks
	to execute ns2. As a result, the ns2Command must take the file to
	write the tracefile to askits last argument. Use something like the
	following in your TCL script:
		
	set tracefd	[open [lindex $argv [expr $argc-1]] w]
	"""
	
	class TempCleaner( file ):
		def close( self ):
			try:
				# Attempt to delete the file
				os.remove( self.name )
			except OSError:
				pass
			
			file.close( self )
			
		def __del__( self ):
			try:
				os.remove( self.name )
			except OSError:
				pass
	
	# Set a SIGCHLD signal handler so that the call to open()
	# will return if the child dies
	signal.signal( signal.SIGCHLD, sigchldHandler )
	
	try:
		# Create the FIFO in a loop
		# This must be done because another process could create the
		# temp file between when we get the name and create it
		while True:
			fifoName = tempfile.mktemp()
			try:
				os.mkfifo( fifoName, 0600 )
				break
			except OSError, value:
				# Ignore the EEXIST error and try to create the FIFO again
				if value[0] != errno.EEXIST: raise
		
		# Before we start ns2, let's make sure that this file actually exists
		assert( os.access( fifoName, os.F_OK ) );
		
		# Start ns2 with the FIFO as its output
		arguments = ns2Command.split()
		arguments.append( fifoName )
			
		#~ print "arguments =", arguments
		pid = os.fork()
		if pid == 0:
			# Child
			try:
				#~ print "child", arguments
				
				# Close stdin, stdout, and stderr
				os.close( 0 )
				os.close( 1 )
				os.close( 2 )
				
				# Redirect stdin, stdout and stderr to /dev/null
				os.open("/dev/null", os.O_RDONLY)    # standard input (0)
				os.open("/dev/null", os.O_RDWR)       # standard output (1)
				os.open("/dev/null", os.O_RDWR)       # standard error (2)
				os.execvp( arguments[0], arguments )
			except:
				#~ print "child exception"
				#~ print sys.exc_info()[2]
				sys.exit( 1 )
			
			# we should never get here
			raise "Unknown error"

		#~ print "ns2 pid =", pid
		#~ import time
		#~ time.sleep( 0.1 )
		code = os.waitpid( pid, os.WNOHANG )
		if code[0] == pid:
			code = code[1]
			assert( False )
		
		# This will not return until a writer is connected to the FIFO
		# TODO: Use a non-blocking open and select to time this out eventually
		signal.alarm( 5 )
		try:
			ns2DataPipe = file( fifoName, "r", 4096*16 )
		except IOError, value:
			if value[0] == errno.EINTR:
				# ns2 quit on us
				assert( False )
			else:
				raise
		signal.alarm( 0 )
		
		# We have the FIFO open, and a writer is connected: we no longer need it
		os.unlink( fifoName )
				
		# Clean up the subprocess
		# TODO: How can we do this in a tricky fashion?
		#~ code = os.waitpid( pid, 0 )
		#~ assert( code[0] == pid and os.WIFEXITED( code[1] ) and os.WEXITSTATUS( code[1] ) == 0 )		
	finally:
		if os.path.exists( fifoName ):
			os.unlink( fifoName )
		
		# Remove the SIGCHLD signal handler
		signal.signal( signal.SIGCHLD, signal.SIG_DFL )

	return pid,ns2DataPipe

def executeAndParseScript( script, arguments = "", trimFromFront = 5, trimFromBack = 5 ):
	"""Executes the specified ns2 script using executeAndParse and a
	temporary file."""
	
	scriptFile = tempfile.NamedTemporaryFile()
	scriptFile.write( script )
	scriptFile.flush()
	
	#~ import shutil
	#~ shutil.copy( scriptFile.name, "temp" )
	
	results = executeAndParse( "ns %s %s" % ( scriptFile.name, arguments ), trimFromFront, trimFromBack )
	
	scriptFile.close()
	
	return results

def executeAndParse( ns2Command, trimFromFront = 5, trimFromBack = 5 ):
	"""Executes the specified ns2 command and parses the results via
	parseAggregateStatistics. The output from ns2 is directly read into
	Python. This function creates a FIFO, then forks to execute ns2. As a
	result, the ns2Command must take the file to write the tracefile to as
	its last argument. Use something like the following in your TCL script:
		
	set tracefd	[open [lindex $argv [expr $argc-1]] w]
	"""
	
	# Create the FIFO
	pid,ns2DataPipe = executeNs2( ns2Command )
	
	# Go and parse the FIFO until it is closed
	results = parseAggregateStatistics( ns2DataPipe, trimFromFront, trimFromBack )
	ns2DataPipe.close()
	
	# Clean up the subprocess
	code = os.waitpid( pid, 0 )
	assert( code[0] == pid and os.WIFEXITED( code[1] ) and os.WEXITSTATUS( code[1] ) == 0 )		

	return results


def sweepInterval( ns2FileName, start, end, increment, args="", trimStart=5, trimEnd=5 ):
	"""Execute ns2 a number of times with a range of parameters, and
	return the aggregate results from parseAggregateStatistics as a 2D
	list. The start, end, and increment parameters are passed to frange()
	to generate the sequence of values. The command executed is:
	
	ns <ns2FileName> <args> <dataValue>"""
	
	results = []	
	for interval in frange.frange( start, end, increment ):
		l = [ interval ]
		l.extend( executeAndParse( "ns %s %s %f" % ( ns2FileName, args, interval ), trimStart, trimEnd ) )
		results.append( l )
	
	return results


def smartSweepInterval( ns2FileName, start, end, increment, args="", dropThreshold=0.10 ):
	"""Similar to sweepInterval(), this function executes ns2 a number of
	times, each time passing it the specified data value. However, if the
	drop ratio ever exceeds dropThreshold, this function returns the data
	collected so far. This is useful if you are searching for the maximum
	sustainable throughput, or something similar."""
	
	# We want to start from the LONGEST interval
	# which is the slowest sending rate
	if start < end:
		temp = end
		end = start
		start = temp
		increment = -increment
	
	assert( start > end )
	assert( increment < 0 )
	
	results = []
	for interval in frange.frange( start, end, increment ):
		
		l = [ interval ]
		l.extend( executeAndParse( "ns %s %s %f" % ( ns2FileName, args, interval ) ) )
		results.append( l )
		
		if drops > dropThreshold:
			break
	
	return results


def locateMaxThroughput( ns2FileName, args="" ):
	"""Performs a binary search for the maximum aggregate throughput. The
	packet interval will get appended to the file name, after the arguments
	are appended. Don't use this function: use smartSweepInterval instead."""
	
	minAdjustment = 0.0001
	
	acceptableDrops = 10
	
	maxInterval = 1
	maxIntervalStats = executeAndParse( "ns %s %s %f" % ( ns2FileName, args, maxInterval ) )
	# The maximum interval must have no drops
	assert( maxIntervalStats[1] == 0 )
	
	maximumInterval = maxInterval
	maxThroughput = maxIntervalStats[0]
	maxDrops = maxIntervalStats[1]
	
	minInterval = 0.001
	minIntervalStats = executeAndParse( "ns %s %s %f" % ( ns2FileName, args, minInterval ) )
	# The minimum interval must have drops
	assert( minIntervalStats[1] > acceptableDrops )
	# The minimum interval must have a higher throughput than the max interval
	assert( minIntervalStats[0] > maxIntervalStats[0] )
	
	while maxInterval - minInterval > 2*minAdjustment:
		interval = maxInterval - (maxInterval - minInterval) / 2
		( throughput, drops, delay ) = executeAndParse( "ns %s %s %f" % ( ns2FileName, args, interval ) )
		
		#~ print "%f: %f %d" % ( interval, throughput, drops )
		if drops < acceptableDrops:
			#~ assert( drops >= maxIntervalStats[1] )
			maxInterval = interval
			maxIntervalStats = ( throughput, drops )
			
			if throughput > maxThroughput:
				maxThroughput = throughput
				maxDrops = drops
				maximumInterval = interval
		else:
			#~ assert( drops < minIntervalStats[1] + acceptableDrops*3 )
			minInterval = interval
			minIntervalStats = ( throughput, drops )
	
	return maxThroughput


if __name__ == '__main__':
	if len( sys.argv ) != 2:
		print "ns2stats.py <trace file>"
		print
		print "\tPrints the aggregate throughput, drop ratio, and average delay"
		sys.exit( 1 )
		
	trace = file( sys.argv[1] )
	throughput, drops, delay, flows = parseAggregateStatistics( trace )
	trace.close()
	
	print "Throughput (bytes/s)\tDrop Ratio\tAverage Delay (s)"
	print "%f\t%f\t%f" % ( throughput, drops, delay )
	print
	
	for flowId, values in flows.iteritems():
		print "%d -> %d:\t%f\t%f\t%f" % ( flowId[0], flowId[1], values[0], values[1], values[2] )
	print
