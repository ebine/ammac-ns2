// Draws an ns2 wireless network.
//
// Originally written by Evan Jones <ejones@uwaterloo.ca> Februrary, 2004
// http://evanjones.ca/software/ns2tools.html
//
// Ported to GnuStep/Linux by Cecil M. Reid.
//
//
// Mac OS X:
// gcc --std=c99 -g -o drawnetwork drawnetwork.m -framework Cocoa
//
// GnuStep: Use this GNUmakefile:
//
// include $(GNUSTEP_MAKEFILES)/common.make
// APP_NAME = DrawNet
// ADDITIONAL_OBJCFLAGS += --std=c99 
// DrawNet_OBJC_FILES = drawnetwork.m
// include $(GNUSTEP_MAKEFILES)/application.make
//
// Released under the "do whatever you want" public domain licence.

#include <Cocoa/Cocoa.h>
#include <Foundation/Foundation.h>
#include <AppKit/AppKit.h>

@interface DrawNetwork : NSView
{
	const NSPoint* nodes;
	size_t numNodes;
	
	const int** routes;
	size_t numRoutes;
}

- (BOOL)isOpaque;
- (void)drawRect: (NSRect) rect;
+ (DrawNetwork*)createNetworkWithNodes: (const NSPoint*) nodes count: (size_t)count frame: (NSRect) frame;
+ (DrawNetwork*)createNetworkWithNodes: (const NSPoint*) n count: (size_t)nodeCount routes: (const int**) r count: (size_t) routeCount frame: (NSRect) frame;
@end

void drawNode( size_t number, const NSPoint centre, NSColor* color )
{
	NSString* label = [NSString stringWithFormat: @"%d", number];
	
	NSSize dimensions = [label sizeWithAttributes: nil];
	float radius = dimensions.width;
	if ( dimensions.height > dimensions.width )
	{
		radius = dimensions.height;
	}
	radius = (radius / 2.0) + 3;

	NSBezierPath* path = [NSBezierPath bezierPathWithOvalInRect: NSMakeRect( centre.x - radius, centre.y -radius, radius * 2, radius * 2 )];	
	if ( color == nil )
	{
		[[NSColor whiteColor] set];
	}
	else
	{
		[color set];
	}
	[path fill];

	[[NSColor blackColor] set];
	[path stroke];
		
	[label drawAtPoint: NSMakePoint( centre.x - dimensions.width / 2, centre.y - dimensions.height / 2 ) withAttributes: nil];
}

double NSPointDistance( NSPoint a, NSPoint b )
{
	double distanceX = a.x - b.x;
	double distanceY = a.y - b.y;
	return sqrt( distanceX*distanceX + distanceY*distanceY );
}

void routeNodes( NSPoint a, NSPoint b )
{
	[NSBezierPath setDefaultLineWidth: 2.0];
	[[NSColor blackColor] set];
	[NSBezierPath strokeLineFromPoint: a toPoint: b];
}

void interferenceNodes( NSPoint a, NSPoint b )
{
	[NSBezierPath setDefaultLineWidth: 0.5];
	[[NSColor redColor] set];
	[NSBezierPath strokeLineFromPoint: a toPoint: b];
}

@implementation DrawNetwork
+ (DrawNetwork*)createNetworkWithNodes: (const NSPoint*) n count: (size_t)count frame: (NSRect) frame
{
	return [DrawNetwork createNetworkWithNodes: n count: count routes: NULL count: 0 frame: frame];
}

+ (DrawNetwork*)createNetworkWithNodes: (const NSPoint*) n count: (size_t)nodeCount routes: (const int**) r count: (size_t) routeCount frame: (NSRect) frame
{
	DrawNetwork* v = [[DrawNetwork alloc] initWithFrame: frame];
	
	v->nodes = n;
	v->numNodes = nodeCount;
	v->routes = r;
	v->numRoutes = routeCount;
	
	return v;
}

- (BOOL) isOpaque
{
	return YES;
}

- (void) drawRect: (NSRect) rect
{
	NSAffineTransform* transform = [NSAffineTransform transform];
	[transform translateXBy: 50 yBy: 50];
	[transform scaleBy: 0.9];
	
	[transform concat];
	
	
	[NSBezierPath setDefaultLineWidth: 1.0];
	[[NSColor blackColor] set];
	[NSBezierPath strokeRect: rect];
	
	if ( numRoutes == 0 )
	{
		for ( size_t i = 0; i < numNodes; ++ i )
		{
			for ( size_t j = i+1; j < numNodes; ++ j )
			{
				// Connect the two nodes if they are within 550m
				double distance = NSPointDistance( nodes[i], nodes[j] );
				if ( distance < 550 )
				{
					// Connect with black line if < 250m, red line otherwise
					if ( distance < 250 )
					{
						routeNodes( nodes[i], nodes[j] );
					}
					else
					{
						interferenceNodes( nodes[i], nodes[j] );
					}
				}
			}
		}
		
		for ( size_t i = 0; i < numNodes; ++ i )
		{
			drawNode( i, nodes[i], nil );
		}	
	}
	else
	{
		// Draw the paths
		for ( size_t i = 0; i < numRoutes; ++ i )
		{
			int last = -1;
			const int* route = routes[i];
			while ( *route != -1 )
			{
				for ( size_t j = 0; j < numNodes; ++ j )
				{
					double distance = NSPointDistance( nodes[*route], nodes[j] );
					if ( distance < 550 )
					{
						interferenceNodes( nodes[*route], nodes[j] );
					}
				}
				
				if ( last != -1 )
				{
					routeNodes( nodes[last], nodes[*route] );
				}
				last = *route;
				++ route;
			}
		}
		
		// Draw the nodes
		int* drawnNodes = calloc( numNodes, sizeof(int) );
		for ( size_t i = 0; i < numRoutes; ++ i )
		{
			if ( ! drawnNodes[ routes[i][0] ] )
			{
				drawNode( routes[i][0], nodes[ routes[i][0] ], [NSColor greenColor] );
				drawnNodes[ routes[i][0] ] = 1;
			}
			
			int last = -1;
			const int* route = routes[i];
			while ( *route != -1 )
			{
				last = *route;
				++ route;
			}
			
			// Draw the destination node
			if ( ! drawnNodes[ last ] )
			{
				drawNode( last, nodes[ last ], [NSColor redColor] );
				drawnNodes[ last ] = 1;
			}
		}
		
		// draw the nodes on the routes
		for ( size_t i = 0; i < numRoutes; ++ i )
		{
			
			int last = -1;
			const int* route = routes[i];
			while ( *route != -1 )
			{
				if ( ! drawnNodes[ *route ] )
				{
					drawNode( *route, nodes[ *route ], nil );
					drawnNodes[ *route ] = 1;
				}
			
				last = *route;
				++ route;
			}
		}
		
		// draw the remaining nodes
		for ( size_t i = 0; i < numNodes; ++ i )
		{
			if ( ! drawnNodes[ i ] )
			{
				drawNode( i, nodes[ i ], [[NSColor grayColor] colorWithAlphaComponent: 0.4] );
				drawnNodes[ i ] = 1;
			}
		}
		free( drawnNodes );
		drawnNodes = NULL;
	}
}
@end

int main( int argc, char* argv[] )
{
	[NSApplication sharedApplication];
	
	NSString* fileContents = nil;
	
	if ( argc != 2 && argc != 3 )
	{
		printf( "drawnetwork [input script] [output pdf (optional)]\n" );
		return 1;
	}
	
	NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
	
	fileContents = [NSString stringWithContentsOfFile: [NSString stringWithCString: argv[1] ] ];
	NSArray* commands = [fileContents componentsSeparatedByString: @"\n"];
	
	NSString* outputFile = nil;
	if ( argc == 3 )
	{
		outputFile = [NSString stringWithCString: argv[2] ];
	}
	else
	{
		outputFile = [[NSString stringWithCString: argv[1] ] stringByDeletingPathExtension];
#ifdef __APPLE__
		// Mac OS X: Default to PDF output
		outputFile = [outputFile stringByAppendingPathExtension: @"pdf"];
#else
		// GnuStep: Default to EPS since PDF doesn't work?
		outputFile = [outputFile stringByAppendingPathExtension: @"eps"];
#endif
	}
	
	NSSize size;
	
	int nodeCount = 0;
	int routeCount = 0;

	for ( size_t i = 0; i < [commands count]; ++ i )
	{
		if ( [(NSString*) [commands objectAtIndex: i] length] == 0 )
		{
			continue;
		}
		
		if ( [(NSString*) [commands objectAtIndex: i] cString][0] == '#' )
		{
			continue;
		}
		
		NSArray* tokens = [[commands objectAtIndex: i] componentsSeparatedByString: @"\t"];
		NSString* command = [tokens objectAtIndex: 0];

		if ( [command isEqualToString: @"area"] )
		{
			if ( [tokens count] != 3 )
			{
				printf( "Error at line %lu: Incorrect number of tokens (%u)\n", i+1, [tokens count] );
				return 2;
			}
			double x = [[tokens objectAtIndex: 1] doubleValue];
			double y = [[tokens objectAtIndex: 2] doubleValue];
			size = NSMakeSize( x, y );
		}
		else if ( [command isEqualToString: @"node"] )
		{
			if ( [tokens count] != 3 )
			{
				printf( "Error at line %lu: Incorrect number of tokens (%u)\n", i+1, [tokens count] );
				return 2;
			}
			++ nodeCount;
		}
		else if ( [command isEqualToString: @"route"] )
		{
			if ( [tokens count] < 3 )
			{
				printf( "Error at line %zd: Incorrect number of tokens (%d)\n", i+1, [tokens count] );
				return 2;
			}
			++ routeCount;
		}
		else
		{
			printf( "Unknown command: %s\n", [command cString] );
		}
	}
	
	if ( nodeCount == 0 )
	{
		printf( "No nodes!\n" );
		return 3;
	}
	
	NSPoint* nodes = malloc( nodeCount * sizeof( NSPoint ) );
	if ( nodes == NULL )
	{
		printf( "Malloc failed\n" );
		return 5;
	}
	
	int** routes = NULL;
	if ( routeCount != 0 )
	{
		routes = malloc( routeCount * sizeof( int* ) );
		if ( routes == NULL )
		{
			printf( "Malloc failed\n" );
			return 5;
		}
	}
	
	size_t nodeIterator = 0;
	size_t routeIterator = 0;
	for ( size_t i = 0; i < [commands count]; ++ i )
	{
		if ( [(NSString*) [commands objectAtIndex: i] length] == 0 )
		{
			continue;
		}
		
		NSArray* tokens = [[commands objectAtIndex: i] componentsSeparatedByString: @"\t"];
		
		NSString* command = [tokens objectAtIndex: 0];

		if ( [command isEqualToString: @"node"] )
		{
			double x = [[tokens objectAtIndex: 1] doubleValue];
			double y = [[tokens objectAtIndex: 2] doubleValue];
			
			if ( ! (0 <= x && x <= size.width ) )
			{
				printf( "Node %d X value out of range\n", nodeCount );
				return 7;
			}
			if ( ! (0 <= y && y <= size.height ) )
			{
				printf( "Node %d Y value out of range\n", nodeCount );
				return 8;
			}
			
			nodes[nodeIterator] = NSMakePoint( x, y );
			++ nodeIterator;
		}
		else if ( [command isEqualToString: @"route"] )
		{
			routes[routeIterator] = malloc( ([tokens count]) * sizeof(int) );
			if ( routes[routeIterator] == NULL )
			{
				printf( "Malloc failed\n" );
				return 9;
			}
			
			// Read all the integers
			for (size_t i = 1; i < [tokens count]; ++i)
			{
				routes[routeIterator][i-1] = [[tokens objectAtIndex: i] intValue];
				if ( ! (0 <= routes[routeIterator][i-1] && routes[routeIterator][i-1] < nodeCount) )
				{
					printf( "Route %zd has invalid node value: %d\n", routeIterator, routes[routeIterator][i-1] );
				}
			}
			// Terminate the route
			routes[routeIterator][[tokens count]-1] = -1;
			
			++ routeIterator;
		}
	}

	NSRect frame = NSMakeRect( 0, 0, size.width, size.height );
	NSView* view = [DrawNetwork createNetworkWithNodes: nodes count: nodeCount routes: (const int**) routes count: routeCount frame: frame];
	
	BOOL success = NO;
	NSData* data = nil;
	if ([[outputFile pathExtension] isEqual: @"pdf"])
	{
		NSMutableData* mutableData = [[NSMutableData alloc] init];
		NSPrintOperation* print = [NSPrintOperation PDFOperationWithView: view insideRect: frame toData: mutableData];
		success = [print runOperation];
		data = mutableData;
	}
	else if ([[outputFile pathExtension] isEqual: @"eps"])
	{
		NSMutableData* mutableData = [[NSMutableData alloc] init];
		NSPrintOperation* print = [NSPrintOperation EPSOperationWithView: view insideRect: frame toData: mutableData];
		success = [print runOperation];
		data = mutableData;
	}
	else if ([[outputFile pathExtension] isEqual: @"png"])
	{
		NSImage* image = [[NSImage alloc] initWithSize: size];
		[image lockFocus];
		[view drawRect: frame];
		NSBitmapImageRep* bitmap = [[NSBitmapImageRep alloc] initWithFocusedViewRect: frame];
		[image unlockFocus];
		[image release];
		data = [bitmap representationUsingType:NSPNGFileType properties:nil];
		[bitmap release];
		success = YES;
	}
	else
	{
		fprintf(stderr, "Unsupported file type: %s\n",
			[[outputFile pathExtension] cStringUsingEncoding: NSUTF8StringEncoding]);
		success = NO;
	}
	
	if (success)
	{
		[[NSFileManager defaultManager]
			createFileAtPath: outputFile
			contents: data
			attributes: nil];
	}
	else
	{
		fprintf(stderr, "Error rendering image\n");
	}

	// According to leaks, this is not a leak! Must be autoreleased by the pool?
	//~ [view release];
	//~ [data release];
	[pool release];
	
	free( nodes );
	nodes = NULL;
	if ( routes != NULL )
	{
		for (int i = 0; i < routeCount; ++i)
		{
			free(routes[i]);
		}
		free( routes );
		routes = NULL;
	}
	
	// Uncomment to use leaks on this process
	//~ fread( &argc, 1, 1, stdin );
}
