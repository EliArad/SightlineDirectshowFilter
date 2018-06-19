//------------------------------------------------------------------------------
// File: SLA-3000.cpp
//
 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <streams.h>
#include "SLA-3000.h"



CallbackContextType myCallbackContext;
WSADATA wsaData;


SLAFilterParams  m_slaFilterParams;

CSLA3000* g_foo_ptr = NULL;

void __cdecl myStatsCallback(SLCapStats *stats, void *context)
{
	CallbackContextType *p = (CallbackContextType*)context;
	//p->display->Stats = *stats; // Update statistics for ShowStats.
	//if (g_foo_ptr) // Check before calling
	//		g_foo_ptr->FooButtonCallback(a, b, c, d);
}
 

int ReadParamsteres()
{
	 

	char line[500];
	FILE *handle = fopen("c:\\sla3000.json" , "r+t");
	if (handle != NULL)
	{
		while (!feof(handle))
		{
			char *p = fgets(line, 500, handle);
			if (p == NULL)
				return 1;
		
			const char s[2] = ":";
			char *token;
			token = strtok(line, s);
			char *data = strtok(NULL, s);
			if (strcmp(token, "addr") == 0)
			{
				strcpy(m_slaFilterParams.Addr, data);
			}
		}

		return 1;
	}
	return 0;

}



void __cdecl myCallback(void *context, SLAImage *image, KLVData *klv, KLVData *recent)
{
	CallbackContextType *p = (CallbackContextType*)context;
	//if (g_foo_ptr) // Check before calling
//		g_foo_ptr->FooButtonCallback(a, b, c, d);

}
 

//------------------------------------------------------------------------------
// Name: CSLA3000::CSLA3000(()
// Desc: Constructor for the ball class. The default arguments provide a
//       reasonable image and ball size.
//------------------------------------------------------------------------------
CSLA3000::CSLA3000(int iImageWidth, int iImageHeight, int iBallSize) :
    m_iImageWidth(iImageWidth),
    m_iImageHeight(iImageHeight),
    m_iBallSize(iBallSize),
    m_iAvailableWidth(iImageWidth - iBallSize),
    m_iAvailableHeight(iImageHeight - iBallSize),
    m_x(0),
    m_y(0),
    m_xDir(RIGHT),
    m_yDir(UP)
{
    // Check we have some (arbitrary) space to bounce in.
    ASSERT(iImageWidth > 2*iBallSize);
    ASSERT(iImageHeight > 2*iBallSize);

    // Random position for showing off a video mixer
    m_iRandX = rand();
    m_iRandY = rand();

	WSAStartup(MAKEWORD(2, 2), &wsaData);
	
	myCallbackContext.decoder = new SLADecode();

	//  following code sets up the decode library
	int rv;
	//char ADDR[] = "udp://@224.10.10.10:15004"; // Multicast address
	//char ADDR[] = "udp://@224.10.10.10:5004"; // Multicast address
	//char ADDR[] = "udp://@239.255.0.1:1234"; // Multicast address
	//char ADDR[] = "udp://@192.168.1.124:15004"; // Unicast address
	//char ADDR[] = "video1.ts"; // Recorded file
	//char ADDR[] = "udp://@:15004"; // Unicast to this PC
	char ADDR[] = "udp://@:5004"; // Unicast to this PC

	//char ADDR[] = "rtsp://root:root@192.168.1.153:554/mpeg4/media.amp";
	//char ADDR[] = "http://root:root@192.168.1.153:80/axis-cgi/mjpg/video.cgi?resolution=4CIF";
	// -i http://151cam.uoregon.edu/axis-cgi/mjpg/video.cgi?camera=1&resolution=640x480
	//char ADDR[] = "V:\InsituClip026.MP4";

	strcpy(m_slaFilterParams.Addr, "udp://@:5004");

	ReadParamsteres();


	// Create the decoder to decode specified stream
	if (myCallbackContext.decoder->Create(ADDR, myCallback, myStatsCallback, &myCallbackContext) < 0)
	{
		::MessageBox(NULL, L"Failed to initialize SLA 3000", L"Sightline SLA 3000", 0);
		return;
	}

#if 0 
	myCallbackContext.decoder->StopSaving();
	if (rv) {
		
		::MessageBox(NULL, L"Error Creating SLA Decode" , L"Sightline SLA 3000" , 0);
	}
#endif 

} // (Constructor)




//------------------------------------------------------------------------------
// Name: CSLA3000::PlotBall()
// Desc: Positions the ball on the memory buffer.
//       Assumes the image buffer is arranged as Row 1,Row 2,...,Row n
//       in memory and that the data is contiguous.
//------------------------------------------------------------------------------
void CSLA3000::PlotBall(BYTE pFrame[], BYTE BallPixel[], int iPixelSize)
{
    ASSERT(m_x >= 0);
    ASSERT(m_x <= m_iAvailableWidth);
    ASSERT(m_y >= 0);
    ASSERT(m_y <= m_iAvailableHeight);
    ASSERT(pFrame != NULL);
    ASSERT(BallPixel != NULL);

    // The current byte of interest in the frame
    BYTE *pBack;
    pBack = pFrame;     

    // Plot the ball into the correct location
    BYTE *pBall = pFrame + ( m_y * m_iImageWidth * iPixelSize) + m_x * iPixelSize;

    for(int row = 0; row < m_iBallSize; row++)
    {
        for(int col = 0; col < m_iBallSize; col++)
        {
            // For each byte fill its value from BallPixel[]
            for(int i = 0; i < iPixelSize; i++)
            {  
                if(WithinCircle(col, row))
                {
                    *pBall = BallPixel[i];
                }
                pBall++;
            }
        }
        pBall += m_iAvailableWidth * iPixelSize;
    }

} // PlotBall


//------------------------------------------------------------------------------
// CSLA3000::BallPosition()
// 
// Returns the 1-dimensional position of the ball at time t millisecs
//      (note that millisecs runs out after about a month!)
//------------------------------------------------------------------------------
int CSLA3000::BallPosition(int iPixelTime, // Millisecs per pixel
                        int iLength,    // Distance between the bounce points
                        int time,       // Time in millisecs
                        int iOffset)    // For a bit of randomness
{
    // Calculate the position of an unconstrained ball (no walls)
    // then fold it back and forth to calculate the actual position

    int x = time / iPixelTime;
    x += iOffset;
    x %= 2 * iLength;

    // check it is still in bounds
    if(x > iLength)
    {    
        x = 2*iLength - x;
    }
    return x;

} // BallPosition


//------------------------------------------------------------------------------
// CSLA3000::MoveBall()
//
// Set (m_x, m_y) to the new position of the ball.  move diagonally
// with speed m_v in each of x and y directions.
// Guarantees to keep the ball in valid areas of the frame.
// When it hits an edge the ball bounces in the traditional manner!.
// The boundaries are (0..m_iAvailableWidth, 0..m_iAvailableHeight)
//
//------------------------------------------------------------------------------
void CSLA3000::MoveBall(CRefTime rt)
{
    m_x = BallPosition(10, m_iAvailableWidth, rt.Millisecs(), m_iRandX);
    m_y = BallPosition(10, m_iAvailableHeight, rt.Millisecs(), m_iRandY);

} // MoveBall


//------------------------------------------------------------------------------
// CSLA3000:WithinCircle()
//
// Return TRUE if (x,y) is within a circle radius S/2, center (S/2, S/2)
//      where S is m_iBallSize else return FALSE
//------------------------------------------------------------------------------
inline BOOL CSLA3000::WithinCircle(int x, int y)
{
    unsigned int r = m_iBallSize / 2;

    if((x-r)*(x-r) + (y-r)*(y-r)  < r*r)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }

} // WithinCircle


