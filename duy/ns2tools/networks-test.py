#!/usr/bin/python
#
# Tests for the networks module

import unittest
import math

from networks import *

class NodeTest( unittest.TestCase ):
	def testDistance( self ):
		n1 = Node( 0, 0 )
		n2 = Node( 1.0, 0 )
		assert( n1.distance( n2 ) == n2.distance( n1 ) )
		assert( n1.distance( n2 ) == 1 )
		
		n2 = Node( 0, 0 )
		assert( n1.distance( n2 ) == n2.distance( n1 ) )
		assert( n1.distance( n2 ) == 0 )
		
		n2 = Node( 0, -2.0 )
		assert( n1.distance( n2 ) == n2.distance( n1 ) )
		assert( n1.distance( n2 ) == 2.0 )
		
		n1 = Node( 50.0, 50.0 )
		n2 = Node( 100.0, 25.0 )
		assert( n1.distance( n2 ) == n2.distance( n1 ) )
		assert( n1.distance( n2 ) == math.sqrt( 50*50 + 25*25 ) )

class NetworkBasicsTest( unittest.TestCase ):
	def testCreate( self ):
		# Edge case testing
		self.assertRaises( ValueError, Network, (-1, 1000) )
		self.assertRaises( ValueError, Network, (0, 1000) )
		self.assertRaises( ValueError, Network, (1000, -1) )
		self.assertRaises( ValueError, Network, (1000, 0) )
		self.assertRaises( ValueError, Network, (1000, 1000), 0 )
		self.assertRaises( ValueError, Network, (1000, 1000), -1 )
		
		network = Network( (1000, 1000), 250 )
		id = network.addNode( 0, 0 )
		assert( len( network.nodes ) == 1 )
		assert( network.nodes[id].x == 0 and network.nodes[id].y == 0 )
		
		id = network.addNode( 100, 100 )
		assert( len( network.nodes ) == 2 )
		assert( network.nodes[id].x == 100 and network.nodes[id].y == 100 )
		
		id = network.addNode( 1000, 1000 )
		assert( len( network.nodes ) == 3 )
		assert( network.nodes[id].x == 1000 and network.nodes[id].y == 1000 )
		
		self.assertRaises( ValueError, network.addNode, 1000, 1000.1 )
		self.assertRaises( ValueError, network.addNode, -0.1, 1000 )
	
	def testUnconnectedNodes( self ):
		network = Network( (1000, 1000), 100 )
		network.addNode( 0, 0 )		
		network.addNode( 100, 0 ) # just in range
		last = network.addNode( 200.1, 0 ) # just out of range
		
		nodes = network.unconnectedNodes()
		
		assert( len( nodes ) == 1 )
		assert( nodes[0] == network.nodes[last] )
		
		network = Network( (1000, 1000) )
		network.addNode( 0, 0 )		
		network.addNode( 100, 0 )
		last = network.addNode( 200.1, 0 )
		
		nodes = network.unconnectedNodes()
		assert( len( nodes ) == 0 )
	
	def testMaximumConnectedSet( self ):
		network = Network( (1000, 1000), 100 )
		first = network.addNode( 0, 0 )		
		second = network.addNode( 100, 0 ) # just in range
		third = network.addNode( 50, 50 )
		last = network.addNode( 250.1, 0 ) # out of range
		
		nodes = network.maximumConnectedSet()
		assert( len( nodes ) == 3 )
		assert( network.nodes[first] in nodes )
		assert( network.nodes[second] in nodes )
		assert( network.nodes[third] in nodes )
		
		nodes = network.unconnectedNodes()
		assert( len( nodes ) == 1 )
		assert( nodes[0] == network.nodes[last] )
		
		network.addNode( 300, 0 ) # in range of last
		
		nodes = network.maximumConnectedSet()
		assert( len( nodes ) == 3 )
		assert( network.nodes[first] in nodes )
		assert( network.nodes[second] in nodes )
		assert( network.nodes[third] in nodes )
	
		nodes = network.unconnectedNodes()
		assert( len( nodes ) == 0 )
	
class NetworkRoutingTest( unittest.TestCase ):
	def setUp( self ):
		# Create a 3x3 grid network
		self.network = Network( (400, 400) )
		
		for x in range( 0, 600, 200 ):
			for y in range( 0, 600, 200 ):
				self.network.addNode( x, y )
		
		assert( len( self.network.nodes ) == 9 )
	
	def validatePath( self, path, src, dst, length ):
		assert( len( path ) == length )
		assert( path.source() == self.network.nodes[src] )
		assert( path.last() == self.network.nodes[dst] )
	
	def validateSingle( self, results, src, dst ):
		assert( len( results ) == 1 ) # One hop = one path
		self.validatePath( results[0], src, dst, 2 ) # one hop path = 2 nodes
	
	def validateNoExtras( self, routeFunction ):
		#errors
		self.assertRaises( IndexError, routeFunction, 9, 8 )
		
		# zero hops
		results = routeFunction( 0, 0 )
		assert( len( results ) == 1 )
		assert( len( results[0] ) == 1 )
		assert( results[0].path[0] == self.network.nodes[0] )
		
		# Single hop, no extras
		results = routeFunction( 0, 1 )
		self.validateSingle( results, 0, 1 )
		results = routeFunction( 4, 7 )
		self.validateSingle( results, 4, 7 )
		results = routeFunction( 8, 5 )
		self.validateSingle( results, 8, 5 )
		
		# three hops, no extras
		results = routeFunction( 0, 7 )
		assert( len( results ) == 3 )
		for r in results:
			self.validatePath( r, 0, 7, 4 )

		# four hops, no extras: 
		results = routeFunction( 0, 8 )
		assert( len( results ) == 6 )
		for r in results:
			self.validatePath( r, 0, 8, 5 )
		results = routeFunction( 6, 2 )
		assert( len( results ) == 6 )
		for r in results:
			self.validatePath( r, 6, 2, 5 )
	
	def validateExtras( self, routeFunction ):
		# zero hops: extras make no difference
		results = routeFunction( 5, 5, 1000 )
		assert( len( results ) == 1 )
		assert( len( results[0] ) == 1 )
		assert( results[0].path[0] == self.network.nodes[5] )
		
		# Single hop, one extra hop: makes no difference in a grid
		results = routeFunction( 0, 1, 1 )
		self.validateSingle( results, 0, 1 )
		results = routeFunction( 4, 7, 1 )
		self.validateSingle( results, 4, 7 )
		results = routeFunction( 8, 5, 1 )
		self.validateSingle( results, 8, 5 )
		
		# single hop, two extras: more paths
		results = routeFunction( 0, 1, 2 )
		assert( len( results ) == 2 )
		self.validatePath( results[0], 0, 1, 2 )
		self.validatePath( results[1], 0, 1, 4 )
		results = routeFunction( 4, 7, 2 )
		assert( len( results ) == 3 )
		self.validatePath( results[0], 4, 7, 2 )
		self.validatePath( results[1], 4, 7, 4 )
		self.validatePath( results[2], 4, 7, 4 )
		assert( results[1] != results[2] )
		
		# three hops, 2 extras
		results = routeFunction( 0, 7, 2 )
		assert( len( results ) == 8 )
		for r in results[3:]:
			self.validatePath( r, 0, 7, 6 )
		
		# four hops, one extras: no difference
		results = routeFunction( 0, 8, 1 )
		assert( len( results ) == 6 )
		for r in results:
			self.validatePath( r, 0, 8, 5 )
		results = routeFunction( 6, 2, 1 )
		assert( len( results ) == 6 )
		for r in results:
			self.validatePath( r, 6, 2, 5 )
	
	def testFindShortestPaths( self ):
		routeFunction = lambda src, dst: self.network.findShortestPaths( src, dst, 0 )
		self.validateNoExtras( routeFunction )
		
		self.validateExtras( self.network.findShortestPaths )
	
	def testFindShortestPathsHeuristic( self ):
		routeFunction = lambda src, dst: self.network.findShortestPathsHeuristic( src, dst, 0 )
		self.validateNoExtras( routeFunction )
		
		# Heuristic routing is interesting: it does not find
		# paths that backtrack to one or two hop neighbours.
		#~ self.validateExtras( self.network.findShortestPathsHeuristic )
		
		# zero hops: extras make no difference
		results = self.network.findShortestPathsHeuristic( 5, 5, 1000 )
		assert( len( results ) == 1 )
		assert( len( results[0] ) == 1 )
		assert( results[0].path[0] == self.network.nodes[5] )
		
		# Single hop, one extra hop: makes no difference in a grid
		results = self.network.findShortestPathsHeuristic( 0, 1, 1 )
		self.validateSingle( results, 0, 1 )
		results = self.network.findShortestPathsHeuristic( 4, 7, 1 )
		self.validateSingle( results, 4, 7 )
		results = self.network.findShortestPathsHeuristic( 8, 5, 1 )
		self.validateSingle( results, 8, 5 )
		
		# four hops, one extras: no difference
		results = self.network.findShortestPathsHeuristic( 0, 8, 1 )
		assert( len( results ) == 6 )
		for r in results:
			self.validatePath( r, 0, 8, 5 )
		results = self.network.findShortestPathsHeuristic( 6, 2, 1 )
		assert( len( results ) == 6 )
		for r in results:
			self.validatePath( r, 6, 2, 5 )
	
	def testFindShortestPathsOnly( self ):
		self.validateNoExtras( self.network.findShortestPathsOnly )
	
	def testRouteAll( self ):
		self.network.routeAll()
		
		routeFunction = lambda src, dst: self.network.nodes[src].routes[dst]
		self.validateNoExtras( routeFunction )

unittest.main() 
