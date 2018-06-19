#pragma once 
//------------------------------------------------------------------------------
// File: Ball.h
//
// Desc: DirectShow sample code - header file for the bouncing ball
//       source filter.  For more information, refer to Ball.cpp.
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#include "SLADecode.h"
//------------------------------------------------------------------------------
// Define GUIDS used in this sample
//------------------------------------------------------------------------------
// {4FAB76CA-E33B-4b52-849F-0A3C732D37A0}
DEFINE_GUID(CLSID_SightLineSLA3000,
	0x4fab76ca, 0xe33b, 0x4b52, 0x84, 0x9f, 0xa, 0x3c, 0x73, 0x2d, 0x37, 0xa0);


// Context for callback function
typedef struct {
	HWND wnd;
	SLADecode *decoder;
	//SLADisplayGDI *display;
} CallbackContextType;

typedef struct SLAFilterParams
{
	char Addr[100];

} SLAFilterParams;

int ReadParamsteres();

//------------------------------------------------------------------------------
// Class CSLA3000
//
// This class encapsulates the behavior of the bounching ball over time
//------------------------------------------------------------------------------
class CSLA3000
{
public:

	CSLA3000(int iImageWidth = 320, int iImageHeight = 240, int iBallSize = 10);


    // Plots the square ball in the image buffer, at the current location.
    // Use BallPixel[] as pixel value for the ball.
    // Plots zero in all 'background' image locations.
    // iPixelSize - the number of bytes in a pixel (size of BallPixel[])
    void PlotBall(BYTE pFrame[], BYTE BallPixel[], int iPixelSize);

    // Moves the ball 1 pixel in each of the x and y directions
    void MoveBall(CRefTime rt);

    int GetImageWidth() { return m_iImageWidth ;}
    int GetImageHeight() { return m_iImageHeight ;}

private:

    enum xdir { LEFT = -1, RIGHT = 1 };
    enum ydir { UP = 1, DOWN = -1 };

    // The dimensions we can plot in, allowing for the width of the ball
    int m_iAvailableHeight, m_iAvailableWidth;

    int m_iImageHeight;     // The image height
    int m_iImageWidth;      // The image width
    int m_iBallSize;        // The diameter of the ball
    int m_iRandX, m_iRandY; // For a bit of randomness
    xdir m_xDir;            // Direction the ball
    ydir m_yDir;            // Likewise vertically

    // The X position, in pixels, of the ball in the frame
    // (0 < x < m_iAvailableWidth)
    int m_x;

    // The Y position, in pixels, of the ball in the frame
    // (0 < y < m_iAvailableHeight)
    int m_y;    

    // Return the one-dimensional position of the ball at time T milliseconds
    int BallPosition(int iPixelTime, int iLength, int time, int iOffset);

    /// Tests a given pixel to see if it should be plotted
    BOOL WithinCircle(int x, int y);

}; // CSLA3000
