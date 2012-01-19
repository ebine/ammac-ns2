#!/usr/bin/python

"""Provides a function for extracting the throughput over time. If directly
executed, it prints the throughput over time in a tab separated table."""

import sys

import flowanalysis

def throughputOverTime( flows ):
	"""Returns a table of the throughput over time."""
	
	headers = [ "Time" ]
	data = []
	
	start = 100000
	end = 0
	for flow in flows:
		if flow.startTime() < start:
			start = flow.startTime()
		if flow.finishTime() > end:
			end = flow.finishTime()
	
	end = float( int( end + 0.5 ) ) # round up
	start = float( int( start ) ) # round down
	
	for flow in flows:
		#~ print "Flow %d:%d -> %d:%d" % ( flow._sourceAddr, flow._sourcePort, flow._destinationAddr, flow._destinationPort )
		#~ print "Start: %f" % ( flow.startTime() )
		#~ print "End: %f" % ( flow.finishTime() )
		#~ print "Bytes: %d" % ( flow.bytesReceived() )
		#~ print "Throughput: %f bytes/s" % ( flow.throughput() )
		#~ print "Dropped Packets: %d" % ( flow.droppedPackets() )
		#~ print
		headers.append( "%d:%d -> %d:%d" % ( flow._sourceAddr, flow._sourcePort, flow._destinationAddr, flow._destinationPort ) )
		
		throughput = flow.throughput( 1.0, start, end )
		for i, row in enumerate( throughput ):
			if i < len( data ):
				if data[i][0] != row[0]:
					raise Exception, "Initial values do not match"
				
				data[i].append( row[1] )
			else:
				data.append( row )
				
	data[0:0] = [ headers ]
	
	return data

if __name__ == "__main__":
	if len( sys.argv ) != 2:
		print "throughputOverTime.py <trace file>"
		print
		print "\tLocates all flows in the file and prints their throughput over time."
		sys.exit( 1 )
		
	trace = file( sys.argv[1] )
	flows = flowanalysis.parseFlowsFromTrace( trace )
	trace.close()
	
	for row in throughputOverTime( flows ):
		print "\t".join( [ str( col ) for col in row ] )
