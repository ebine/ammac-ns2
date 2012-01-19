#!/usr/bin/python

"""This module contains objects that help generate wireless networks to be fed
into ns2. It supports generating random topologies, shortest path routing via
Dijkstra's algorithm and outputting the network data in ns2 format."""

import sys
import math
import random

class Node( object ):
	"""This class represents a single wireless node. The important
	attributes are the x and y coordinates. The others are designed to be
	internal attributes used for routing."""
	__slots__ = ('x','y','neighbours', 'shortestPathLength', 'shortestPaths', 'routes')
	
	def __init__( self, x, y ):
		'''Creates a new Node located at the specified x and y coordinates.'''
		self.x = x
		self.y = y
		self.neighbours = None
		self.shortestPathLength = -1
		self.shortestPaths = None
		
		# This maps destinations to lists of paths
		self.routes = {}
	
	#~ def __cmp__( self, other ):
		#~ if other == None:
			#~ return 1
		#~ val = cmp( self.x, other.x )
		#~ if ( val != 0 ): return val
		
		#~ return cmp( self.y, other.y )
	
	#~ def __hash__( self ):
		#~ return hash( self.x ) ^ hash( self.y )

	def distance( self, other ):
		"""Returns the distance from this node to another node."""
		diffX = self.x - other.x
		diffY = self.y - other.y
		return math.sqrt( diffX*diffX + diffY*diffY )

class Path( object ):
	"""Represents a path of connected nodes. The node instances can be
	directly accessed via the self.path list."""
	__slots__ = ('path','visited')
	
	def __init__( self, source ):
		self.path = [ source ]
		self.visited = { source: 1 }
		#~ self.neighbours = None
	
	def __cmp__( self, other ):
		if other == None:
			return 1
		return cmp( self.path, other.path )
	
	def __len__( self ):
		return len( self.path )
	
	def append( self, node ):
		assert( not self.visited.has_key( node ) )
		
		self.visited[node] = 1
		self.path.append( node )

	def isNodeDisjoint( self, other ):
		'''Returns True if this path does not share any nodes with the
		other path. It returns False if it shares at least one node.'''
		for node in self.path:
			if node in other.visited:
				return False
		
		return True

	def clone( self ):
		"""Returns a clone of this object with new instances for the 
		first level variables (not a recursive copy)."""
		
		shallowCopy = Path( None )
		shallowCopy.path = list( self.path )
		shallowCopy.visited = dict( self.visited )
		
		return shallowCopy
	
	def reverse( self ):
		"""Return a clone of this path in reverse order."""
		
		backwards = self.clone()
		#~ print "before", backwards.path
		backwards.path.reverse()
		#~ print "after", backwards.path
		return backwards

	def distance( self ):
		"""Returns the total distance of the path, computed as the
		sum of the distances between each node."""
		
		last = self.path[0]
		distance = 0
		for p in self.path[1:]:
			distance += last.distance( p )
			last = p
		
		return distance

	def source( self ):
		return self.path[0]
	
	def last( self ):
		return self.path[-1]
	
	def unvisitedPaths( self ):
		unvisitedPaths = []
		for neighbour in self.path[-1].neighbours:
			if not self.visited.has_key( neighbour ):
				# Unvisited neighbour: make a copy of this path and append the neighbour
				c = self.clone()
				c.append( neighbour )
				
				unvisitedPaths.append( c )
		return unvisitedPaths

class ListSubclassFifo(list):
	"""A list subclass that provides better performance for enqueue and
	dequeue operations.
	
	From: http://aspn.activestate.com/ASPN/Cookbook/Python/Recipe/68436
	- Constant time enqueue and dequeue
	- Has a higher constant than the list
	- Only faster if you are dequeuing from lists with more than ~1000 items"""
	__slots__ = ('front',)
	
	def __init__(self):
		self.front = []
	
	def enqueue(self, elt):
		self.append( elt )

	def dequeue(self):
		if not self.front:
			self.reverse()
			self.front = self[:]
			self[:] = []
	
		return self.front.pop()
		
	def __iter__(self):
		# Iterate until and index exception is thrown by dequeue
		try:
			while 1:
				yield self.dequeue()
		except IndexError:
			pass
	
	def __len__(self):
		return list.__len__( self ) + len( self.front )
		
	def dequeueEnd( self ):
		if not list.__len__( self ):
			self.front.reverse()
			self[:] = self.front
			self.front = []

		return self.pop()
	
	def peekAtEnd( self ):
		if list.__len__( self ):
			return self[-1]
		else:
			return self.front[0]

class HeuristicPath2( Path ):
	"""This Path subclass is used to make Dijkstra's algorithm run a bit faster."""
	def append( self, node ):
		Path.append( self, node )
		
		# Add the neighbours of the last node to the visited list
		for neighbour in self.path[-2].neighbours:
			self.visited[neighbour] = 1
		# Add the 2nd hop neighbours of the 2nd last node to the visited list
		if len( self.path ) >= 3:
			for neighbour in self.path[-3].neighbours:
				for hopNeighbour in neighbour.neighbours:
					self.visited[hopNeighbour] = 1
	
	def clone( self ):
		shallowCopy = HeuristicPath2( None )
		shallowCopy.path = list( self.path )
		shallowCopy.visited = dict( self.visited )
		
		return shallowCopy

def randomNetwork( size, numNodes=None, density=None ):
	"""Generates a random connected network, and computes the neighbour information."""
	if numNodes == None and density == None:
		raise ValueError, "Must specifiy one of numNodes OR density"
	if numNodes != None and density != None:
		raise ValueError, "Must specify only one of numNodes OR density, not both"
	
	if ( numNodes == None ):
		numNodes = int( size[0]*size[1]*density + 0.5 )
	else:
		density = float(size[0]*size[1])/numNodes
	
	network = Network( size )
	
	while numNodes > 0:
		#~ print "Placing %d nodes ..." % (numNodes)
		# Randomly place the nodes
		for i in xrange( numNodes ):
			# Place a random node
			network.addNode( random.uniform( 0, size[0] ), random.uniform( 0, size[1] ) )
		
		# Replace any that aren't connected to other nodes
		numNodes = 0
		for node in network.unconnectedNodes():
			network.nodes.remove( node )
			numNodes += 1
		
		# No lonely nodes: make sure the graph is connected
		if numNodes == 0:
			maxConnected = network.maximumConnectedSet()
			if len( maxConnected ) != len( network.nodes ):
				#~ print "Disconnected graph. Keeping largest connected subset"
				# We have a disconnected graph: Remove all but the largest connected subset
				for node in network.nodes:
					if not node in maxConnected:
						network.nodes.remove( node )
						numNodes += 1
	
	return network

class Network:
	"""Represents a collection of nodes."""
	def __init__( self, size, range = 250 ):
		"""size - A tuple of the (x, y) dimensions of the network terrain.
		range - The maximum distance between connected nodes."""
		if size[0] <= 0:
			raise ValueError, "size[0] must be greater than zero"
		if size[1] <= 0:
			raise ValueError, "size[1] must be greater than zero"
		if range <= 0:
			raise ValueError, "range must be greater than zero"
		
		self.size = size
		self.nodes = []
		self.range = range
		self.neighboursFound = False
	
	def addNode( self, x, y ):
		# Check that the node is within the bounds of the network
		if not (0 <= x and x <= self.size[0]):
			raise ValueError, "x coordinate (%f) out of range [0, %f]" % ( x, self.size[0] ) 
		if not (0 <= y and y <= self.size[1]):
			raise ValueError, "y coordinate (%f) out of range [0, %f]" % ( y, self.size[1] ) 
		
		id = len( self.nodes )
		self.nodes.append( Node( x, y ) )
		self.neighboursFound = False
		
		return id
	
	def findNeighbours( self ):
		"""Recalculates the neighbour lists for each node."""
		for node in self.nodes:
			node.neighbours = []
		
		for id, node in enumerate( self.nodes ):
			# Iterate through all the *remaining* nodes
			# this way we never check any pair of nodes more than once
			for other in self.nodes[id+1:]:
				# The nodes are in range of each other: add them as neighbours
				if ( node.distance( other ) <= self.range ):
					node.neighbours.append( other )
					other.neighbours.append( node )
		
		self.neighboursFound = True
	
	def unconnectedNodes( self ):
		if not self.neighboursFound:
			self.findNeighbours()
		
		return [ node for node in self.nodes if len( node.neighbours ) == 0 ]
		
	def maximumConnectedSet( self ):
		if not self.neighboursFound:
			self.findNeighbours()
		
		visited = {}
		maximumConnectedSet = {}
		
		# Locate the largest connected subset of nodes
		while len( visited ) < len( self.nodes ):
			connectedSet = {}
			nodeStack = []
			
			# Select an unvisited node
			# This could be improved by changing "visited" to be "unvisited"
			for node in self.nodes:
				if not node in visited:
					connectedSet[node] = 1
					nodeStack.append( node )
					break
			assert( len( nodeStack ) > 0 )
			
			
			while len( nodeStack ):
				current = nodeStack.pop()
				
				for neighbour in current.neighbours:				
					if not neighbour in connectedSet:
						# We found a new neighbour: add it to the set
						# And visit it eventually
						connectedSet[neighbour] = 1
						nodeStack.append( neighbour )
			
			# If this connected subset is larger than previous ones, set it as the maximum
			if len( maximumConnectedSet ) < len( connectedSet ):
				maximumConnectedSet = connectedSet
			for key in connectedSet.keys():
				visited[key] = 1
		
		return maximumConnectedSet.keys()
	
	def findShortestPaths( self, source, destination, extraHopLimit = 0 ):
		"""Finds all the shortest paths from source index node to the
		destination index nodes. It will also return paths with up to
		extraHopLimit hops past the shortest path."""
		
		if not self.neighboursFound:
			self.findNeighbours()
		
		paths = []
		shortestPathLength = -1
		queue = ListSubclassFifo()
		queue.enqueue( Path( self.nodes[source] ) )
		
		#~ maxQueueLen = 0
		#~ queueLenSum = 0
		#~ queueLenCounts = 0
		
		# Iterates efficiently (i hope) through the queue
		for current in queue:
			#~ if len( queue ) > maxQueueLen:
				#~ maxQueueLen = len( queue )
			#~ queueLenSum += len( queue )
			#~ queueLenCounts += 1
			
			#~ print "Currently at Node", current.last().x, current.last().y
			
			if ( current.last() == self.nodes[destination] ):
				# We found a valid path: add it
				paths.append( current )
				
				if ( shortestPathLength == -1 ):
					# This is BFS, so the first path we find is the shortest
					shortestPathLength = len( current )
			# The comparison is less than because it will need one more 
			elif ( shortestPathLength == -1 or len( current ) < shortestPathLength + extraHopLimit ):
				# If any other paths will be within the length limit, add to the queue (keep searching)
				newPaths = current.unvisitedPaths()
				for path in newPaths:
					if destination in path.visited:
						print "Destination is excluded from this path"
				queue.extend( newPaths )
		
		#~ print "Queue Length Avg = %f Max = %d" % ( float( queueLenSum ) / queueLenCounts, maxQueueLen )
		
		return paths

	def findShortestPathsHeuristic( self, source, destination, extraHopLimit = 0 ):
		"""Same as findShortestPaths, but it uses the heuristic path
		so it runs much faster. I believe the results should be
		the same, but I have not thought about it enough to be able to
		be certain about it."""
		if not self.neighboursFound:
			self.findNeighbours()
		
		if source == destination:
			return ( Path( self.nodes[source] ), )
		
		paths = []
		shortestPathLength = -1
		queue = ListSubclassFifo()
		queue.enqueue( HeuristicPath2( self.nodes[source] ) )
		
		# Iterates efficiently (i hope) through the queue
		# We dequeue then expand the neighbours:
		# This saves a big chunk of RAM, because the neighbours are not
		# created until needed, and at the end, lots of paths are thrown away
		# It uses less system time, but uses slightly more user time
		for c in queue:
			for current in c.unvisitedPaths():
				#~ print "current path =", self.pathToIndexes( current ),
				
				if ( current.last() == self.nodes[destination] ):
					#~ print "valid"
					
					# We found a valid path: add it
					
					
					paths.append( current )
					
					if ( shortestPathLength == -1 ):
						#~ print "shortest path found", len( current )

						# This is BFS, so the first path we find is the shortest
						shortestPathLength = len( current )
						
						
						
						# Go through the queue backwards, and delete anything that is longer than the limit
						while len( queue) and len( queue.peekAtEnd() ) +1 > shortestPathLength + extraHopLimit:
							queue.dequeueEnd()

				elif shortestPathLength == -1 or len( current ) + 1 <= shortestPathLength + extraHopLimit:
					#~ print "queued"
					
					# If any other paths will be within the length limit, add to the queue (keep searching)
					# testing to make sure that the destination isn't in the visited list was a slight performance LOSS
					queue.append( current )
				#~ else:
					#~ print "discarded"
		
		return paths

	def findShortestPathsOnly( self, source, destination ):
		"""Yet another findShortestPaths, implementation, but this one
		finds only the shortest paths."""
		
		if source == destination:
			return ( Path( self.nodes[source] ), )

		if not self.neighboursFound:
			self.findNeighbours()

		# Make sure all the shortest path data in all the nodes is erased
		for node in self.nodes:
			node.shortestPathLength = sys.maxint
			node.shortestPaths = []
		
		shortestPathLength = sys.maxint
		
		source = self.nodes[source]
		destination = self.nodes[destination]
		
		source.shortestPathLength = 1 # Length is nodes, not hops
		source.shortestPaths = [ ( source, ) ]
		
		queue = ListSubclassFifo()
		queue.enqueue( source )
		# Iterates efficiently (i hope) through the queue
		for node in queue:
			if node == destination:
				break 
			
			# Check to see if we have a shortest path to our neighbours
			for neighbour in node.neighbours:
				if neighbour.shortestPathLength >= node.shortestPathLength + 1:
					# The neighbour is already queued if there are shortest paths
					isQueued = len( neighbour.shortestPaths ) > 0
					
					# Verify that my assumptions about the node visiting order is correct
					if neighbour.shortestPathLength == node.shortestPathLength + 1:
						assert( len( neighbour.shortestPaths ) > 0 )
					else:
						assert( neighbour.shortestPathLength == sys.maxint )
					
					# If we found a shorter path to this neighbour,
					# forget all the other paths
					if neighbour.shortestPathLength > node.shortestPathLength + 1:
						assert( neighbour.shortestPathLength == sys.maxint )
						neighbour.shortestPaths = []
						neighbour.shortestPathLength = node.shortestPathLength + 1
					
					assert( neighbour.shortestPathLength == node.shortestPathLength + 1 )
					
					for path in node.shortestPaths:
						assert( neighbour not in path )
						
						newPath = path + (neighbour,)
						neighbour.shortestPaths.append( newPath )

					# Queue the neighbour to be visited if it isn't already queued
					if not isQueued:
						queue.enqueue( neighbour )
		
		paths = destination.shortestPaths
		
		# Save some memory: go free all the paths we created.
		for node in self.nodes:
			node.shortestPathLength = sys.maxint
			node.shortestPaths = None
		
		# Convert the paths into Path objects
		pathObjs = []
		for pathTuple in paths:
			pathObj = Path( pathTuple[0] )
			for node in pathTuple[1:]:
				pathObj.append( node )
			pathObjs.append( pathObj )

		return pathObjs


	def routeAll( self, maxPaths=None ):
		"""Floyd-Warshall all pairs shortest path. O(n^3)
		Implemented thanks to: http://ai-depot.com/BotNavigation/Path-AllPairs.html
		Computes all the shortest paths in the network. It will
		only do this computation once, and it is faster to do it for
		the entire network at the same time than it is to do if for
		each pair of nodes in order.
		
		After calling this, paths can be found via:
		self.nodes[source].routes[destination]"""
		if not self.neighboursFound:
			self.findNeighbours()
			
		# Creates an NxN table to store all shortest paths
		pathsTable = [ None ] * len( self.nodes )
		for i in xrange( len( self.nodes ) ):
			pathsTable[i] = [ None ] * len( self.nodes )

		# Initialize the table based on the neighbour information
		for nodeIndex, node in enumerate( self.nodes ):
			for neighbour in node.neighbours:
				neighbourIndex = self.nodes.index( neighbour )
				
				# We only do part of the matrix, then we copy and reverse it later
				if neighbourIndex > nodeIndex:
					path = [ node ]
					path.append( neighbour )
					pathsTable[ nodeIndex ][ neighbourIndex ] = [ path ]
					path2 = list( path )
					path2.reverse()
					pathsTable[ neighbourIndex ][ nodeIndex ] = [ path2 ]

		for k in xrange( len( self.nodes ) ):
			for i in xrange( len( self.nodes ) ):
				# We cannot detour i -> k, therefore, the table will be unchanged
				if pathsTable[i][k] == None:
					continue

				ikCost = len( pathsTable[i][k][0] )
					
				for j in xrange( len( self.nodes ) ):
					# Skip non-paths
					if i == j: continue
						
					if pathsTable[k][j] != None:
						kjCost = len( pathsTable[k][j][0] )
						
						newCost = ikCost + kjCost - 1 # Shared node!
						
						ijCost = sys.maxint
						if pathsTable[i][j] != None:
							ijCost = len( pathsTable[i][j][0] )
						
						if ( newCost <= ijCost ):
							# The new detour is as good or better than the old one
							if ( newCost < ijCost ):
								# We found a cheaper path: forget all those old paths
								pathsTable[i][j] = []
							
							# Extend all the i -> k paths by appending the k -> j paths
							for ikPath in pathsTable[i][k]:
								for kjPath in pathsTable[k][j]:
									newPath = list( ikPath )
									newPath.extend( kjPath[1:] )
									
									assert( len( newPath ) == newCost )
									pathsTable[i][j].append( newPath)
							
							if maxPaths != None and len( pathsTable[i][j] ) > maxPaths*10:
								#~ print "pathsTable[%d][%d] has %d paths" % ( i, j, len( pathsTable[i][j] ) )
								# Select a subset of the paths to be kept, the rest will be discarded
								pathsTable[i][j] = random.sample( pathsTable[i][j], maxPaths )
		
		for source in xrange( len( self.nodes ) ):
			for destination in xrange( len( self.nodes ) ):
				pathObjs = None
				
				if source == destination:
					pathObjs = ( Path( self.nodes[source] ), )
				else:
					#~ print "%d -> %d: %d paths" % ( source, destination, len( pathsTable[source][destination] ) )
					pathObjs = []
					for pathlist in pathsTable[source][destination]:
						pathObj = Path( pathlist[0] )
						for node in pathlist[1:]:
							pathObj.append( node )
						pathObjs.append( pathObj )
						#~ print self.pathToIndexes( path )
					
				self.nodes[source].routes[destination] = pathObjs
				pathsTable[source][destination] = None
				#~ self.nodes[source].routes[destination] = pathsTable[source][destination]

	def pathToIndexes( self, path ):
		"""Converts a path object into a list of node indexes."""
		indexes = []
		for node in path.path:
			indexes.append( self.nodes.index( node ) )
		return indexes

	def drawScript( self, paths=None ):
		"""Returns a list of node coordinates in the format for my
		network topology drawing program (drawnetwork)."""
		output = "area\t%f\t%f\n\n" % ( self.size[0], self.size[1] )
		
		for node in self.nodes:
			output += "node\t%f\t%f\n" % ( node.x, node.y )
		output += "\n"
		
		if paths:
			for path in paths:
				output += "route\t"
				output += "\t".join( [ str( index ) for index in self.pathToIndexes( path ) ] )
				output += "\n"

		return output

class NS2ScriptBuilder:
	def __init__( self, network ):
		self.topologyToNsIndex = {}
		self.routeData = ''
		self.network = network
		self.numFlows = 0
	
## Returns the header of the ns2 file with the topology of the network.
##	Routing must be added seperately.
	def getScript( self ):
		ns2File = """
# ======================================================================
# Define options
# ======================================================================
set val(chan)	Channel/WirelessChannel	;# channel type
set val(prop)	Propagation/TwoRayGround	;# radio-propagation model
set val(netif)	Phy/WirelessPhy	;# network interface type
set val(mac)	Mac/802_11	;# MAC type
set val(ifq)	Queue/DropTail/PriQueue	;# interface queue type
set val(ll)	LL	;# link laset val(chan)	Channel/WirelessChannel	;# channel type
set val(ant)	Antenna/OmniAntenna	;# antenna model
set val(ifqlen)	50	;# max packet in ifq
set val(nn)	%d ;# Number of nodes to create
set val(rp)	DSDV	;# routing protocol

if { $argc != 3 && $argc != 2 } {
	puts "ERROR: Supply file.tcl <random seed>  (<packet interval>) <trace file>"
	exit 1
}
ns-random [lindex $argv 0]
set packetInterval [lindex $argv 1]
set tracefile [lindex $argv [expr $argc-1]]

set ns_	[new Simulator]
$ns_ use-newtrace
set tracefd	[open $tracefile w]
$ns_ trace-all $tracefd

Mac/802_11 set dataRate_ 1Mb
#Mac/802_11 set CWMax_ 31
# Disable RTS
Mac/802_11 set RTSThreshold_ 3000
# Switch to the short preamble
#Mac/802_11 set PreambleLength_ 72

Application/Traffic/CBR set random_ 2 ;# Specifies the "small random variation" setting

# set up topography object
set topo       [new Topography]
$topo load_flatgrid %d %d

# Create GOD object
create-god $val(nn)

# Create a new channel
set channel1_ [new $val(chan)]

# configure node

$ns_ node-config -adhocRouting $val(rp) \
	-llType $val(ll) \
	-macType $val(mac) \
	-ifqType $val(ifq) \
	-ifqLen $val(ifqlen) \
	-antType $val(ant) \
	-propType $val(prop) \
	-phyType $val(netif) \
	-channel $channel1_ \
	-topoInstance $topo \
	-agentTrace ON \
	-routerTrace ON \
	-macTrace OFF \
	-movementTrace OFF

for {set i 0} {$i < $val(nn) } {incr i} {
	set node_($i) [$ns_ node]	
	$node_($i) random-motion 0		;# disable random motion
	$node_($i) set X_ 0.0
	$node_($i) set Y_ 0.0
	$node_($i) set Z_ 0.0
}

proc stop {} {
    global ns_ tracefd
    $ns_ flush-trace
    close $tracefd
    $ns_ halt
}
"""		% ( len( self.topologyToNsIndex ), self.network.size[0], self.network.size[1] )
		# The parameters are number of nodes, X x Y dimensions, 

		# Place the nodes
		for node, index in self.topologyToNsIndex.iteritems():
			ns2File += "$node_(%d) set X_ %f\n$node_(%d) set Y_ %f\n" % ( index, node.x, index, node.y )
	
		ns2File += self.routeData
		ns2File += self.ns2Footer()
		
		return ns2File
	
	def ns2UDPAgent( self ):
		return """
# Setup traffic flow
set agent(%d) [new Agent/UDP]
$agent(%d) set packetSize_ 1500
set app(%d) [new Application/Traffic/CBR]
# 1500 - 20 byte IP header = 1480 bytes
$app(%d) set packetSize_ 1480 ;# This size INCLUDES the UDP header
$app(%d) set interval_ $packetInterval	
set sink(%d) [new Agent/Null]

$app(%d) attach-agent $agent(%d)
"""  % ( self.numFlows, self.numFlows, self.numFlows, self.numFlows, self.numFlows, self.numFlows, self.numFlows, self.numFlows )

	def ns2TCPAgent( self, type, rate ):
		if type.startswith( 'Reno' ):
			sender = 'Agent/TCP'
			receiver = 'Agent/TCPSink'
		else:
			assert( type.startswith( 'SACK' ) )
			sender = 'Agent/TCP/Sack1'
			receiver = 'Agent/TCPSink/Sack1'
		
		if type.endswith( 'DelAck' ):
			receiver += '/DelAck'

		retval = """
# 1500 - 20 byte IP header - 40 byte TCP header = 1440 bytes
Agent/TCP set packetSize_ 1440 ;# This size EXCLUDES the TCP header
Agent/TCP set overhead_ 0.012 ;# Add some random time between sends
Agent/TCP set syn_ 0 ;# Disable SYN/ACK simulation: flows start in the connected state

set agent(%(num)d) [new %(sender)s]
set app(%(num)d) [new Application/FTP]
set sink(%(num)d) [new %(receiver)s]

$app(%(num)d) attach-agent $agent(%(num)d)
""" % { 'num': self.numFlows, 'sender': sender, 'receiver': receiver }

		if rate:
			retval += """
set tbf(%(num)d) [new TBF]
$tbf(%(num)d) set bucket_ [expr 1500*8]
$tbf(%(num)d) set rate_ [expr 1.0/$packetInterval*8*1500]
#~ $tbf(%(num)d) set rate_ $packetInterval
$tbf(%(num)d) set qlen_ 100
""" % { 'num': self.numFlows }
		
		return retval

	def addFlow( self, path, UDP=True, tcpType='SACKDelAck', rate=False ):
		topologyString = ""
		
		# Add all the nodes that are needed
		for node in path.path:
			# Allocate a new ns2 index for this node, if required
			if node not in self.topologyToNsIndex:
				index = len( self.topologyToNsIndex )
				self.topologyToNsIndex[node] = index
				
				#~ topologyString += "$node_(%d) set X_ %f\n$node_(%d) set Y_ %f\n" % ( self.topologyToNsIndex[node], node.x, self.topologyToNsIndex[node], node.y )
		
		routes = None
		if UDP:
			routes = self.ns2UDPAgent()
		else:
			routes = self.ns2TCPAgent( tcpType, rate )
					
		if UDP or rate == False:
			routes += "\n$ns_ attach-agent $node_(%d) $agent(%d)\n" % ( self.topologyToNsIndex[ path.source() ], self.numFlows )
		else:
			assert( UDP == False and rate == True )
			routes += "\n$ns_ attach-tbf-agent $node_(%d) $agent(%d) $tbf(%d)\n" % ( self.topologyToNsIndex[ path.source() ], self.numFlows, self.numFlows )
			
		routes += "$ns_ attach-agent $node_(%d) $sink(%d)\n" % ( self.topologyToNsIndex[ path.last() ], self.numFlows )
		routes += "$ns_ connect $agent(%d) $sink(%d)\n" % ( self.numFlows, self.numFlows )
		
		self.routeData += routes
		self.numFlows += 1

	def getNsToTopologyIndex( self ):
		'''Returns a dictionary that maps the ns2 indexes back to topology indexes.'''
		
		inverse = {}
		for key,value in self.topologyToNsIndex.iteritems():
			inverse[ value ] = self.network.nodes.index( key )
		return inverse
	
	def ns2Footer( self ):
		# The footer for all ns2 files
		return """
if { 0.0 < $packetInterval && $packetInterval < 1.0 } {
	for { set i 0 } { $i < %d } { incr i } {
		#~ $ns_ at [expr $i/%d.0 * $packetInterval] "$app($i) start"
		#~ set u [new RandomVariable/Uniform]
		#~ $ns_ at [$u value] "$app($i) start"
		$ns_ at 0.0 "$app($i) start"
	}
} else {
	for { set i 0 } { $i < %d } { incr i } {
		$ns_ at 0.0 "$app($i) start"
	}
}
# Run the simulation for 60 seconds (1 minutes)
$ns_ at 120.0 "stop"
$ns_ run
""" % ( self.numFlows, self.numFlows, self.numFlows )
