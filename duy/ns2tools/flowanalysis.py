#!/usr/bin/python

"""A module to parse NS2 new style traces. This parsing is very generic, and
as a result, it is quite slow. However, it provides a useful framework for
extracting specific data from files, without having to fight with the raw file
format. The typical way to use it is to call parseFlowsFromTrace() on a trace
to return a set of Flow objects in that trace. Then use the functions on the
Flow and PacketEvent objects to get the data you want.

If executed directly, it parses the trace file passed as the first parameter
and prints the throughput and drop ratios for all the flows in the file."""

DROP_REASON = {
	"END": "DROP_END_OF_SIMULATION",
	"COL": "DROP_MAC_COLLISION",
	"DUP": "DROP_MAC_DUPLICATE",
	"ERR": "DROP_MAC_PACKET_ERROR",
	"RET": "DROP_MAC_RETRY_COUNT_EXCEEDED",
	"STA": "DROP_MAC_INVALID_STATE",
	"BSY": "DROP_MAC_BUSY",
	"NRTE": "DROP_RTR_NO_ROUTE i.e no route is available.",
	"LOOP": "DROP_RTR_ROUTE_LOOP i.e there is a routing loop",
	"TTL": "DROP_RTR_TTL i.e TTL has reached zero.",
	"TOUT": "DROP_RTR_QTIMEOUT i.e packet has expired.",
	"CBK": "DROP_RTR_MAC_CALLBACK",
	"IFQ": "DROP_IFQ_QFULL i.e no buffer space in IFQ.",
	"ARP": "DROP_IFQ_ARP_FULL i.e dropped by ARP",
	"OUT": "DROP_OUTSIDE_SUBNET i.e dropped by base stations on receiving routing updates from nodes outside its domain.",
	"---": "This shouldn't happen, but it does",
	}

EVENT_SEND = "s"
EVENT_RECEIVE = "r"
EVENT_DROP = "d"
EVENT_FORWARD = "f"

EVENT_TYPE = {
	EVENT_SEND: "Send",
	EVENT_RECEIVE: "Receive",
	EVENT_DROP: "Drop",
	EVENT_FORWARD: "Forward",
	}
	
TRACE_AGENT = "AGT"
TRACE_ROUTER = "RTR"
TRACE_MAC = "MAC"
	
TAG_SOURCE = "-Is"
TAG_DESTINATION = "-Id"
TAG_SIZE = "-Il"
TAG_TIME = "-t"
TAG_UID = "-Ii"
TAG_FLOWID = "-If"
TAG_TRACELEVEL = "-Nl"
TAG_REASON = "-Nw"
TAG_PACKET_TYPE = "-P"


IP_HEADER_SIZE = 20


class PacketEvent:
	"""Stores information about the beginning and end of each packet, from being sent to being received or dropped."""
	def __init__( self, sentTime, byteSize, uid ):
		"""sentTime - The time this packet was first sent (created).
		byteSize - Size of packet in bytes.
		uid - Used to uniquely identify this packet."""
		
		assert( sentTime >= 0 )
		assert( byteSize > 0 )
		assert( uid >= 0 )
		
		self._sentTime = sentTime
		self._size = byteSize
		self._uid = uid
		
		self._received = 0
		self._finishTime = None
		
		# Stores all drops: maps drop times to reasons
		self._drops = {}
		
	def valid( self ):
		"""A valid packet event is one that has both been sent and
		received or dropped. Returns True or False."""
		
		# Has been received, is valid
		if ( self._received == 1 ):
			assert( self._finishTime != None )
			return 1
	
		assert( self._received == 0 )
		if ( len( self._drops ) > 0 ):
			assert( self._finishTime != None )
			return 1
		
		return 0
	
	def received( self, receivedTime ):
		"""Mark this packet as being received at receivedTime."""
		
		assert( not self._received ) # Must not have been received yet
		assert( receivedTime > self._sentTime )
		
		self._received = 1
		# This will replace the time for any drops
		self._finishTime = receivedTime
	
	def wasDropped( self ):
		"""Returns True if this packet was dropped and never received."""
		return self._received == 0 and len( self._drops ) > 0
		
	def wasReceived( self ):
		"""Returns True if this packet was received at least once."""
		return self._received == 1
	
	def droppedDuplicates( self ):
		"""Returns the number of duplicates that were dropped. It
		returns the total number of drops, if it was received at least
		once. It returns the number of drops minus one if it was not
		received, and it returns 0 if there have been no events.""" 
		
		if ( not self.valid() ):
			return 0
		if ( self._received ):
			return len( self._drops )
		else:
			return len( self._drops ) - 1
	
	def dropped( self, dropTime, dropReason ):
		"""Mark this packet as being dropped at dropTime for dropReason."""
		assert( dropTime >= self._sentTime )
		assert( DROP_REASON.has_key( dropReason ) )
		assert( len( dropReason ) == 3 or len( dropReason ) == 4 )
		
		assert( not self._drops.has_key( dropTime ) )
		
		# Add this drop to the dictionary and take the largest time
		# as the finish time, if the packet has not been received
		self._drops[dropTime] = dropReason
		
		if ( not self._received ):
			dropTimes = self._drops.keys()
			dropTimes.sort()
			self._finishTime = dropTimes[-1]
	
	def delay( self ):
		"""Return the delay experienced by this packet. Will fail if the
		packet has not been received."""
		assert( self._received )
		return self._finishTime - self._sentTime
	
	def sentTimeCompare( self, other ):
		"""Compares two valid packet events based on sent time."""
		assert( self.valid() and other.valid() )
		
		return cmp( self._sentTime, other._sentTime )

	def finishTimeCompare( self, other ):
		"""Compares two valid packet events based on finish time."""
		assert( self.valid() and other.valid() )
		
		return cmp( self._finishTime, other._finishTime )


class Flow:
	"""Stores PacketEvents and performs computations to obtain statistics about the flows."""
	def __init__( self, sourceAddr, sourcePort, destinationAddr, destinationPort ):
		self._packets = {}
		
		assert( not (sourceAddr == destinationAddr and sourcePort == destinationPort ) )
		
		self._sourceAddr = sourceAddr
		self._sourcePort = sourcePort
		self._destinationAddr = destinationAddr
		self._destinationPort = destinationPort
		
		self._startTime = None
		self._finishTime = None
	
	def send( self, sentTime, byteSize, uid ):
		"""Adds a new packet being sent event to this flow."""
		assert( not self._packets.has_key( uid ) )
		
		p = PacketEvent( sentTime, byteSize, uid )
		
		self._packets[uid] = p
		
		# Update the start/end times, if possible
		# This means that even invalid packet events will be recorded
		if ( self._startTime == None or sentTime < self._startTime ): self._startTime = sentTime
		if ( self._finishTime == None or sentTime > self._finishTime ): self._finishTime = sentTime
	
	def receive( self, receivedTime, uid ):
		assert( self._packets.has_key( uid ) )
		
		self._packets[uid].received( receivedTime )
		
		# Update the end times, if possible
		# This means that even invalid packet events will be recorded
		if ( self._finishTime == None or receivedTime > self._finishTime ): self._finishTime = receivedTime
		
	def drop( self, dropTime, dropReason, uid ):
		assert( self._packets.has_key( uid ) )
		
		self._packets[uid].dropped( dropTime, dropReason )
		
		# Update the end times, if possible
		# This means that even invalid packet events will be recorded
		if ( self._finishTime == None or dropTime > self._finishTime ): self._finishTime = dropTime
	
	def validPackets( self ):
		return [ p for p in self._packets.values() if p.valid() ]
	
	def startTime( self ):
		"""Find the lowest start time (among valid packets)."""
		assert( len( self._packets ) > 0 )
		#~ packets = self.validPackets()
		#~ assert( len( packets ) > 0 )
	
		#~ packets.sort( PacketEvent.sentTimeCompare )
		
		#~ return packets[0]._sentTime
		return self._startTime
	
	def finishTime( self ):
		assert( len( self._packets ) > 0 )
		#~ packets = self.validPackets()
		#~ assert( len( packets ) > 0 )
	
		#~ packets.sort( PacketEvent.finishTimeCompare )
		
		#~ return packets[-1]._finishTime
		return self._finishTime
	
	def bytesReceived( self ):
		total = 0
		
		for p in self._packets.values():
			if ( p._received ):
				total += p._size
		
		return total
	
	def droppedPackets( self ):
		dropCount = 0
		for p in self._packets.values():
			if ( p.wasDropped() ):
				dropCount += 1
		return dropCount
	
	def droppedDuplicates( self ):
		dropCount = 0
		for p in self._packets.values():
			dropCount += p.droppedDuplicates()
		return dropCount
	
	def packetsReceivedInRange( self, startTime, stopTime ):
		"""Returns the packets received in the interval [startTime, stopTime] (inclusive)."""
		return [ p for p in self._packets.values() if
			p.valid() and p.wasReceived() and startTime <= p._finishTime and p._finishTime <= stopTime ]
	
	def throughput( self, averagingInterval = None, discardInitial = None, discardFinal = None ):
		"""Returns the throughput of this flow. Averaging interval
		should be the time you want to average the throughput
		over. By default, it is the entire duration of the flow. The
		duration is measured by discarding the first and last five
		seconds, and rounding that duration to an integer."""
		
		assert( len( self._packets ) > 0 )
		
		if averagingInterval == None:
			if discardInitial == None: discardInitial = 5
			if discardFinal == None: discardFinal = 5
			
			start = self.startTime() + discardInitial
			stop = self.finishTime() - discardFinal
			
			# Round the duration to the nearest integer
			duration = float( int( stop - start + 0.5 ) )
			stop = start + duration
			
			if duration <= 0:
				# No duration means no bytes = zero throughput
				return 0
			
			bytes = 0
			for p in self.packetsReceivedInRange( start, stop ):
				bytes += p._size
			
			return bytes / duration
		else:
			assert( averagingInterval > 0 )
			
			if discardInitial == None: discardInitial = self.startTime()
			if discardFinal == None: discardFinal =  self.finishTime()
			
			# Now go find all received packets
			packets = [ p for p in self.validPackets() if p.wasReceived() ]
			packets.sort( PacketEvent.finishTimeCompare )
			
			data = []
			intervalCount = 0
			start = discardInitial
			end = start + averagingInterval
			bytes = 0
			
			for p in packets:
				# If this packet is after the interval, record the data
				while p._finishTime > end:
					data.append( [ end, float( bytes ) / averagingInterval ] )
					
					# Use multiplication to avoid cumulative float errors when adding floats
					intervalCount += 1
					start = discardInitial + intervalCount * averagingInterval
					end = discardInitial + (intervalCount+1) * averagingInterval
					bytes = 0.0
				
				# If the current interval begins where we are supposed to stop, STOP!
				if start >= discardFinal:
					break
				
				# Ignore packets outside the interval
				# The initial packets could start before the interval
				if ( start < p._finishTime ):
					assert( p._finishTime <= end )
					bytes += p._size
			
			# Go and append all the remaining intervals
			while ( start < discardFinal ):
				data.append( [ end, float( bytes ) / averagingInterval ] )
				
				# Use multiplication to avoid cumulative float errors when adding floats
				intervalCount += 1
				start = discardInitial + intervalCount * averagingInterval
				end = discardInitial + (intervalCount+1) * averagingInterval
				bytes = 0.0
			
			return data
		
		assert( 0 )

def parseFlowsFromTrace( lines ):
	"""Parses NS2 new style trace file lines and returns a dictionary of Flows contained in the file."""
	# Flows are uniquely identified by (saddr, sport, daddr, dport)
	# Dictionary maps flow key  to flow objects
	flows = {}

	tagSource = {
		TAG_PACKET_TYPE: None,
		TAG_SOURCE: None,
		TAG_DESTINATION: None,
		TAG_TRACELEVEL: None,
		TAG_TIME: None,
		TAG_SIZE: None,
		TAG_UID: None,
		TAG_REASON: None,
		}
	
	for line in lines:
		data = line.split()
		
		# Skip blank lines
		if len( data ) == 0: continue
		
		#~ tags = dict( zip( data[1::2], data[2::2] ) )
		tags = {}
		for i in range( 1, len( data )-1, 2 ):
			tags[ data[i] ] = data[i+1]
	
		eventType = data[0]
		if ( not EVENT_TYPE.has_key( eventType ) ):
			# Skip liens that we don't understand
			continue
			#~ print "The file does not appear to be a new style NS2 Trace:"
			#~ print "\tInvalid event type '%s'" % ( eventType )
			#~ sys.exit( 1 )
		
		if tags.has_key( TAG_PACKET_TYPE ) and tags[TAG_PACKET_TYPE] == "arp" and not tags.has_key( TAG_SOURCE ):
			# ARP drops may not have the source recorded, so we skip thim
			#~ print line
			continue
		
		flowKey = "%s.%s" % ( tags[TAG_SOURCE], tags[TAG_DESTINATION] )
		
		if ( tags[TAG_TRACELEVEL] == TRACE_AGENT ):
			if ( eventType == EVENT_SEND  ):
				if ( not flows.has_key( flowKey ) ):
					(saddr, sport) = tags[TAG_SOURCE].split( "." )
					(daddr, dport) = tags[TAG_DESTINATION].split( "." )
					flows[flowKey] = Flow( int( saddr ), int( sport ), int( daddr ), int( dport ) )
				flows[flowKey].send( float( tags[TAG_TIME] ), int( tags[TAG_SIZE] ) + IP_HEADER_SIZE, tags[TAG_UID] )
				
			elif ( eventType == EVENT_RECEIVE ):
				#~ print line
				flows[flowKey].receive( float( tags[TAG_TIME] ), tags[TAG_UID] )
		elif ( eventType == EVENT_DROP ):
			#~ print line
			flows[flowKey].drop( float( tags[TAG_TIME] ), tags[TAG_REASON], tags[TAG_UID] )
	
	return flows.values()

if __name__ == '__main__':
	import sys
    
	if len( sys.argv ) != 2:
		print "count-throughput.py <trace file>"
		print
		print "\tThis will automatically locate all flows in the trace file"
		print "\tand report the following statistics:"
		print
		print "\tthroughput\tdrop ratio"
		sys.exit( 1 )
		
	trace = file( sys.argv[1] )
	flows = parseFlowsFromTrace( trace )
	trace.close()
	
	for flow in flows:
		print "Source -> Dest\tThroughput\tDrop Ratio"
		print "%d -> %d\t%f\t%f" % ( flow._sourceAddr, flow._destinationAddr, flow.throughput(), float( flow.droppedPackets() ) / len( flow._packets ) )
