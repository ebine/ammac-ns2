#!/usr/bin/python

# An example script for the "simple.tcl" example wireless ns2 scenario
# This script parses the trace file and produces a graph showing the flow
# throughput over time

import sys

import flowanalysis
import throughputOverTime
import stupidplot

if len( sys.argv ) != 3:
	print "example.py <trace file> <output eps file>"
	sys.exit( 1 )

input = file( sys.argv[1] )
flows = flowanalysis.parseFlowsFromTrace( input )
input.close()

data = throughputOverTime.throughputOverTime( flows )

stupidplot.gnuplotTable( data, sys.argv[2] )
