#!/usr/bin/python

# An example that uses all the modules:
# 1. Generates a random network.
# 2. Finds a shortest path from a random source and random destination
# 3. Runs a TCP transfer between the two.
# 4. Graphs the result.
# 5. Outputs the script to be fed to my network drawing program.

import random
import tempfile

import networks
import ns2stats
import flowanalysis
from throughputOverTime import *
import stupidplot

# 1000x1000m, 25 nodes
net = networks.randomNetwork( (1000, 1000), 25 )

# Select source and destination randomly
length = 0
while length < 3:
	src = random.randrange( len( net.nodes ) ) 
	dst = random.randrange( len( net.nodes ) ) 
	if ( src == dst ): continue

	# Find the shortest paths
	paths = net.findShortestPathsHeuristic( src, dst )
	length = len( paths[0] )

# Create the scenario file
scriptBuilder = networks.NS2ScriptBuilder( net )
scriptBuilder.addFlow( paths[0], UDP=False )

temp = tempfile.NamedTemporaryFile()
temp.write( scriptBuilder.getScript() )
temp.flush()

# Run the TCP simulation
pid,dataPipe = ns2stats.executeNs2( "ns %s 0" % ( temp.name ) )

# Parse the output
flows = flowanalysis.parseFlowsFromTrace( dataPipe )
dataPipe.close()
temp.close()
data = throughputOverTime( flows )

# Graph the data
options = {
	'ylabel': 'Throughput (kBytes/s)',
}
stupidplot.gnuplotTable( data, "out.eps", options )

# Dump the network so we can draw it
temp = file( "out.txt", "w" )
temp.write( net.drawScript( [ paths[0] ] ) )
temp.close()
