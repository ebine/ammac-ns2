#!/usr/bin/python
#
# Benchmarks various routing routines so you can choose appropriately.
# Note: This takes a long time to run.
# 

import timeit
import random
import sys

import networks

def createNxN( n ):
	# Create a 3x3 grid network
	network = networks.Network( (n*200, n*200) )
	
	for x in range( 0, n*200, 200 ):
		for y in range( 0, n*200, 200 ):
			network.addNode( x, y )
	
	return network

def create3x3():
	return createNxN(3)

def createHuge():
	random.seed( 1001 )
	return networks.randomNetwork( (1000, 1000), 36 )

def findAll( func, network ):
	for src in range( len( network.nodes ) ):
		for dst in range( len( network.nodes ) ):
			func( network, src, dst )

def routeAllSingle( network, source, destination ):
	network.routeAll()
	return network.nodes[source].routes[destination]


def routeOne( networkFunction, routeFunction ):
	network = networkFunction()
	
	return routeFunction( network, 0, len( network.nodes ) - 1 )

def routeAll( networkFunction, routeAllFunction ):
	network = networkFunction()
	
	return routeAllFunction( network )
	

ITERATIONS = 500

createFunctions = (
	lambda: createNxN(3),
	lambda: createNxN(6),
	createHuge,
	)
createNames = (
	"3x3 Grid",
	"6x6 Grid",
	"36 Node Random",
	)

routeFunctions = [
	networks.Network.findShortestPaths,
	networks.Network.findShortestPathsHeuristic,
	networks.Network.findShortestPathsOnly,
	routeAllSingle,
	]
names = (
	"findShortestPaths:\t",
	"findShortestPathsHeuristic:",
	"findShortestPathsOnly:\t",
	"routeAll:\t\t",
	)

print "SINGLE PATH"

print "\t\t\t\t",
for n in createNames:
	print "%s\t" % ( n ),
print

for i, fn in enumerate( routeFunctions ):
	print "%s" % ( names[i] ),
	sys.stdout.flush()
	for create in createFunctions:
		t = timeit.Timer( "routeOne( create, fn )", "from __main__ import routeOne, fn, create" )
		time = t.timeit( ITERATIONS )
		
		print "\t%f" %  ( time/ITERATIONS*1000 ),
		sys.stdout.flush()
	print

print
print "ALL PATHS"


# Wrap the routing functions that need to be
routeFunctions = [
	lambda net: findAll( networks.Network.findShortestPaths, net ),
	lambda net: findAll( networks.Network.findShortestPathsHeuristic, net ),
	lambda net: findAll( networks.Network.findShortestPathsOnly, net ),
	networks.Network.routeAll,
	]

ITERATIONS=25

print "\t\t\t\t",
for n in createNames:
	print "%s\t" % ( n ),
print

for i, fn in enumerate( routeFunctions ):
	print "%s" % ( names[i] ),
	sys.stdout.flush()
	for create in createFunctions:
		t = timeit.Timer( "routeAll( create, fn )", "from __main__ import routeAll, fn, create" )
		time = t.timeit( ITERATIONS )
		
		print "\t%f" %  ( time/ITERATIONS*1000 ),
		sys.stdout.flush()
	print

	