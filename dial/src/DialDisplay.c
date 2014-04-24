/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  D I S P L A Y . C                                                                                        *
 *  ==========================                                                                                        *
 *                                                                                                                    *
 *  This is free software; you can redistribute it and/or modify it under the terms of the GNU General Public         *
 *  License version 2 as published by the Free Software Foundation.  Note that I am not granting permission to        *
 *  redistribute or modify this under the terms of any later version of the General Public License.                   *
 *                                                                                                                    *
 *  This is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the                *
 *  impliedwarranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for   *
 *  more details.                                                                                                     *
 *                                                                                                                    *
 *  You should have received a copy of the GNU General Public License along with this program (in the file            *
 *  "COPYING"); if not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111,   *
 *  USA.                                                                                                              *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @file
 *  @brief Handle dial display.
 *  @version $Id: DialDisplay.c 1815 2013-12-23 08:43:10Z chris $
 */

#include <string.h>
#include <gtk/gtk.h>
#include <libintl.h>
#include <stdlib.h>
#include <math.h>
#include "dialsys.h"

static int dialSize				= 3 * 64;		// Saved in the config file
static int dialWidth			= 1;			// Saved in the config file
static int dialHeight			= 1;			// Saved in the config file
static int markerType			= 0;
static int markerStep           = SCALE_4;
static int dialOpacity			= 100;
static int dialGradient			= 0;
static int dialMaxColours;
static int savePosX, savePosY;
static int saveCentreX, saveCentreY;
static char fontName[101]		= "Sans";		// Saved in the config file
static char saveFilePath[PATH_MAX];
static GtkWindow *mainWindow;
static GtkWidget *drawingArea;
static cairo_t *saveCairo;
static COLOUR_DETAILS *dialColours;
#if GTK_MAJOR_VERSION == 2
static GdkDrawable *windowShapeBitmap = NULL;
static GdkColormap *colourMap;
#endif

/**********************************************************************************************************************
 * Use tables for the sin and cos calculation, it is faster.                                                          *
 **********************************************************************************************************************/
static double sinTable[SCALE_4];
static double cosTable[SCALE_4];

/**********************************************************************************************************************
 * Function prototypes.                                                                                               *
 **********************************************************************************************************************/
void(*UpdateFunc)(void)	= NULL; 
void(*DialSave)(char *path) = NULL;
void dialWindowMask (void);
int dialCreateColours();
void dialFillSinCosTables (int start);

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  I N I T                                                                                                  *
 *  ================                                                                                                  *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Intalize the dail display system.
 *  @param mainWindowIn Main window to use.
 *  @param updatevoid Function to call on update.
 *  @param colourDetails Colours to use on the display.
 *  @param start Offse of zero on the dial (0 = 12:00).
 *  @result A drawingArea used by the face.
 */
GtkWidget *dialInit (GtkWindow *mainWindowIn, void(*update)(void), COLOUR_DETAILS *colourDetails, int start, void *dialSave)
{
	mainWindow = mainWindowIn;
	UpdateFunc = update;
	dialColours = colourDetails;
	dialMaxColours = dialCreateColours();
	dialFillSinCosTables (start);
	DialSave = dialSave;
	
	configGetIntValue ("face_size", &dialSize);
	configGetIntValue ("number_cols", &dialWidth);
	configGetIntValue ("number_rows", &dialHeight);
	configGetIntValue ("marker_type", &markerType);
	configGetIntValue ("marker_step", &markerStep);
	configGetIntValue ("opacity", &dialOpacity);
	configGetIntValue ("gradient", &dialGradient);
	configGetValue ("font_name", fontName, 100);
	
	drawingArea = gtk_drawing_area_new ();
	gtk_widget_set_size_request (drawingArea, dialWidth * dialSize, dialHeight * dialSize);
	
	dialWindowMask();
	return drawingArea;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  D R A W  S T A R T                                                                                       *
 *  ===========================                                                                                       *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Start the drawing of a dial.
 *  @param cr Cairo context saved for later.
 *  @param posX X position of the gauge.
 *  @param posY Y position of the gauge.
 *  @result None.
 */
void dialDrawStart (cairo_t *cr, int posX, int posY)
{
	savePosX = posX;
	savePosY = posY;
	saveCentreX = posX + (dialSize >> 1);
	saveCentreY = posY + (dialSize >> 1);
	saveCairo = cr;
	
	cairo_save (saveCairo);
	cairo_set_line_cap (saveCairo, CAIRO_LINE_CAP_BUTT);
	cairo_set_line_join (saveCairo, CAIRO_LINE_JOIN_MITER);	
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  D R A W  F I N I S H                                                                                     *
 *  =============================                                                                                     *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Finish the drawing of a dial.
 *  @result None.
 */
void dialDrawFinish ()
{
	cairo_restore (saveCairo);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  C O L O U R                                                                                              *
 *  ====================                                                                                              *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Get the colour value for a colour number.
 *  @param i The number of the colour to find.
 *  @result Pointer to the GtkColour.
 */
#if GTK_MAJOR_VERSION == 2
GdkColor *dialColour (int i)
#else
GdkRGBA *dialColour (int i)
#endif
{
	if (i < 0 || i >= dialMaxColours)
		return &dialColours[0].dialColour;
		
	return &dialColours[i].dialColour;
}

void dialSetColour (int i)
{
#if GTK_MAJOR_VERSION == 2
	gdk_cairo_set_source_color (saveCairo, dialColour (i));
#else
	gdk_cairo_set_source_rgba (saveCairo, dialColour (i));
#endif
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  W I N D O W  M A S K                                                                                     *
 *  =============================                                                                                     *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Create a mask for the dials.
 *  @result None.
 */
void dialWindowMask (void)
{
#if GTK_MAJOR_VERSION == 2

	int fullHeight, fullWidth, i, j;
	GdkGC *gc;
        
	fullHeight = dialHeight * dialSize;
	fullWidth = dialWidth * dialSize;
        
	if (windowShapeBitmap)
	{
		g_object_unref (windowShapeBitmap);
		windowShapeBitmap = NULL;
	}

	windowShapeBitmap = (GdkBitmap *) gdk_pixmap_new (NULL, fullWidth, fullHeight, 1);

	gc = gdk_gc_new (windowShapeBitmap);
	gdk_gc_set_foreground (gc, dialColour (0));
	gdk_gc_set_background (gc, dialColour (1));

	gdk_draw_rectangle (windowShapeBitmap, gc, TRUE, 0, 0, fullWidth, fullHeight);

	gdk_gc_set_foreground (gc, dialColour (1));
	gdk_gc_set_background (gc, dialColour (0));

	for (i = 0; i < dialWidth; i++)
	{
		for (j = 0; j < dialHeight; j++)
			gdk_draw_arc (windowShapeBitmap, gc, TRUE, (i * dialSize) - 1, (j * dialSize) - 1, 
					dialSize + 1, dialSize + 1, 0, 360 * 64);
	}
	gtk_widget_shape_combine_mask (GTK_WIDGET (mainWindow), windowShapeBitmap, 0, 0);
	g_object_unref (gc);

#else

	cairo_t *cr;
	cairo_surface_t *surface;
	cairo_region_t *region;
	int fullHeight, fullWidth, i, j;

	fullHeight = dialHeight * dialSize;
	fullWidth = dialWidth * dialSize;

	surface = cairo_image_surface_create (CAIRO_FORMAT_A8, fullWidth, fullHeight);
	cr = cairo_create (surface);
	if (cairo_status (cr) == CAIRO_STATUS_SUCCESS)
	{
		for (i = 0; i < dialWidth; i++)
		{
			for (j = 0; j < dialHeight; j++)
			{
				int centerX = (dialSize * i) + (dialSize >> 1);
				int centerY = (dialSize * j) + (dialSize >> 1);
				
				cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 1.0);
		 		cairo_arc (cr, centerX, centerY, (dialSize * 64) >> 7, 0, 2 * M_PI);
				cairo_fill (cr);
				cairo_stroke (cr);
			}
		}
		region = gdk_cairo_region_create_from_surface (surface);
		gtk_widget_shape_combine_region (GTK_WIDGET (mainWindow), region);
		cairo_region_destroy (region);
	}
	cairo_destroy (cr);

#endif
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  D R A W  M I N U T E                                                                                     *
 *  =============================                                                                                     *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Draw a small minute marker.
 *  @param size Size of the marker.
 *  @param len Length of the marker.
 *  @param angle Angle of the marker.
 *  @param colour colour of the marker.
 *  @result None.
 */
void dialDrawMinute (int size, int len, int angle, int colour)
{
	dialDrawMinuteX (saveCentreX, saveCentreY, size, len, angle, colour);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  D R A W  M I N U T E  X                                                                                  *
 *  ================================                                                                                  *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Draw a small minute marker.
 *  @param posX Centre position.
 *  @param posY Centre position.
 *  @param size Size of the marker.
 *  @param len Length of the marker.
 *  @param angle Angle of the marker.
 *  @param colour colour of the marker.
 *  @result None.
 */
void dialDrawMinuteX (int posX, int posY, int size, int len, int angle, int colour)
{
	cairo_set_line_width (saveCairo, 1.0f + ((float)dialSize / 512.0f));
	dialSetColour (colour);
	if (len < size)
	{
		cairo_move_to (saveCairo,
				posX + dialSin ((dialSize * size) >> 6, angle),
				posY - dialCos ((dialSize * size) >> 6, angle));

		cairo_line_to (saveCairo,
				posX + dialSin ((dialSize * (size + len)) >> 6, angle),
				posY - dialCos ((dialSize * (size + len)) >> 6, angle));
	}
	else
	{
		cairo_move_to (saveCairo,
				posX + dialSin ((dialSize * size) >> 6, angle + SCALE_2),
				posY - dialCos ((dialSize * size) >> 6, angle + SCALE_2));

		cairo_line_to (saveCairo,
				posX + dialSin ((dialSize * (size + len)) >> 6, angle),
				posY - dialCos ((dialSize * (size + len)) >> 6, angle));
	}
	cairo_stroke (saveCairo);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  D R A W  C I R C L E                                                                                     *
 *  =============================                                                                                     *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Draw a circle on the dial.
 *  @param size Size of the marker.
 *  @param colFill Fill colour.
 *  @param colOut Outline colour.
 *  @result None.
 */
void dialDrawCircle (int size, int colFill, int colOut)
{
	dialDrawCircleX (saveCentreX, saveCentreY, size, colFill, colOut);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  D R A W  C I R C L E  X                                                                                  *
 *  ================================                                                                                  *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Draw a circle on the dial.
 *  @param posX Centre position.
 *  @param posY Centre position.
 *  @param size Size of the marker.
 *  @param colFill Fill colour.
 *  @param colOut Outline colour.
 *  @result None.
 */
void dialDrawCircleX (int posX, int posY, int size, int colFill, int colOut)
{
	int trueSize = (dialSize * size) >> 7;
	
	cairo_set_line_width (saveCairo, 1.0f + ((float)dialSize / 256.0f));
	if (colFill != -1)
	{
		dialSetColour (colFill);
 		cairo_arc (saveCairo, posX, posY, trueSize, 0, 2 * M_PI);
		cairo_fill (saveCairo);
		cairo_stroke (saveCairo);
	}
	if (colOut != -1)
	{
		dialSetColour (colOut);
 		cairo_arc (saveCairo, posX, posY, trueSize, 0, 2 * M_PI);
		cairo_stroke (saveCairo);
	}
}

void drawCircleGradient (int size, int colFill, int style)
{
	drawCircleGradientX (saveCentreX, saveCentreY, size, colFill, style);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D R A W  C I R C L E  G R A D I E N T                                                                             *
 *  =====================================                                                                             *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief .
 *  @param cr .
 *  @param posX .
 *  @param posY .
 *  @param size .
 *  @param colFill .
 *  @param style .
 *  @result .
 */
void drawCircleGradientX (int posX, int posY, int size, int colFill, int style)
{
	cairo_pattern_t *pat;
	int patSize = (dialSize >> 1) + (posX > posY ? posX : posY), i;
	int x = posX / dialSize, y = posY / dialSize, j = x + y, k = patSize / dialSize;
	float gradL = (float)(100 - dialGradient) / 100.0;
	float gradH = (float)(100 + dialGradient) / 100.0;
	float x1, x2, col[3][3];

#if GTK_MAJOR_VERSION == 2
	col[0][0] = (float)dialColour(colFill) -> red / 65535.0;
	col[1][0] = (float)dialColour(colFill) -> green / 65535.0;
	col[2][0] = (float)dialColour(colFill) -> blue / 65535.0;
#else
	col[0][0] = dialColour(colFill) -> red;
	col[1][0] = dialColour(colFill) -> green;
	col[2][0] = dialColour(colFill) -> blue;
#endif

	for (i = 0; i < 3; ++i)
	{
		col[i][1] = col[i][0] * (style ? gradH : gradL);
		if (col[i][1] > 1) col[i][1] = 1;
		col[i][2] = col[i][0] * (style ? gradL : gradH);
		if (col[i][2] > 1) col[i][2] = 1;
	}
	pat = cairo_pattern_create_linear (0.0, 0.0, patSize, patSize);
	
	x1 = j;
	x1 /= (2 * k);
	x2 = x1 + ((float)1 / k);
	
	cairo_pattern_add_color_stop_rgb (pat, x1, col[0][1], col[1][1], col[2][1]);
	cairo_pattern_add_color_stop_rgb (pat, x2, col[0][2], col[1][2], col[2][2]);
	cairo_arc (saveCairo, posX, posY, (dialSize * size) >> 7, 0, 2 * M_PI);
	cairo_set_source (saveCairo, pat);
	cairo_fill (saveCairo);
	cairo_stroke (saveCairo);

	cairo_pattern_destroy (pat);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  H O T  C O L D                                                                                           *
 *  =======================                                                                                           *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Display Hot and cold markers.
 *  @param size Radius of the marker.
 *  @param colFill Colour of the marker.
 *  @param cold Hot or Cold, 1 = cold.
 *  @result None.
 */
void dialHotCold (int size, int colFill, int cold)
{
	dialHotColdX (saveCentreX, saveCentreY, size, colFill, cold);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  H O T  C O L D  X                                                                                        *
 *  ==========================                                                                                        *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Display Hot and cold markers.
 *  @param posX Centre posision.
 *  @param posY Centre posision.
 *  @param size Radius of the marker.
 *  @param colFill Colour of the marker.
 *  @param cold Hot or Cold, 1 = cold.
 *  @result None.
 */
void dialHotColdX (int posX, int posY, int size, int colFill, int cold)
{
	int trueSize = (dialSize * size) >> 7;
	
	cairo_set_line_width (saveCairo, 1.0f + ((float)dialSize / 18.0f));
	dialSetColour (colFill);
	if (cold)
		cairo_arc (saveCairo, posX, posY, trueSize, (M_PI * 3) / 4, M_PI);
	else
		cairo_arc (saveCairo, posX, posY, trueSize, 0, M_PI / 4);
	cairo_stroke (saveCairo);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  D R A W  S Q U A R E                                                                                     *
 *  =============================                                                                                     *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Draw a square on the dial.
 *  @param size Size of the marker.
 *  @param colFill Fill colour.
 *  @param colOut Outline colour.
 *  @result None.
 */
void dialDrawSquare (int size, int colFill, int colOut)
{
	dialDrawSquareX (savePosX, savePosY, size, colFill, colOut);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  D R A W  S Q U A R E  X                                                                                  *
 *  ================================                                                                                  *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Draw a square on the dial.
 *  @param posX Centre position.
 *  @param posY Centre position.
 *  @param size Size of the marker.
 *  @param colFill Fill colour.
 *  @param colOut Outline colour.
 *  @result None.
 */
void dialDrawSquareX (int posX, int posY, int size, int colFill, int colOut)
{
	int trueSize = (dialSize * size) >> 6;
	
	cairo_set_line_width (saveCairo, 1.0f + ((float)dialSize / 256.0f));
	if (colFill != -1)
	{
		dialSetColour (colFill);
		cairo_rectangle (saveCairo, posX, posY, trueSize, trueSize);
		cairo_fill (saveCairo);
		cairo_stroke (saveCairo);
	}
	if (colOut != -1)
	{
		dialSetColour (colOut);
		cairo_rectangle (saveCairo, posX, posY, trueSize, trueSize);
		cairo_stroke (saveCairo);
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  D R A W  H A N D                                                                                         *
 *  =========================                                                                                         *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Draw a hand on the dial.
 *  @param angle Angle of the hand.
 *  @param handStyle Style structure of the hand.
 *  @result None.
 */
void dialDrawHand (int angle, HAND_STYLE *handStyle)
{
	dialDrawHandX (saveCentreX, saveCentreY, angle, handStyle);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  D R A W  H A N D  X                                                                                      *
 *  ============================                                                                                      *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Draw a hand on the dial.
 *  @param posX Centre position.
 *  @param posY Centre position.
 *  @param angle Angle of the hand.
 *  @param handStyle Style structure of the hand.
 *  @result None.
 */
void dialDrawHandX (int posX, int posY, int angle, HAND_STYLE *handStyle)
{
	int points[20], i, j, numPoints, fill = 0;
	int size = handStyle -> length, style = handStyle -> style, tail = handStyle -> tail;
	int colFill = handStyle -> fill, colOut = handStyle -> line;

	if (handStyle -> gauge)
	{
		if (angle < 0) angle = 0;
		if (angle >= SCALE_4) angle = SCALE_4 - 1;
	}
	else
	{
		while (angle < 0) angle += SCALE_4;
		while (angle >= SCALE_4) angle -= SCALE_4;
	}

	switch (style)
	{
	case 0:
		// Original double triangle
		points[0] = posX + dialSin ((dialSize * tail) >> 6, angle + SCALE_2);
		points[1] = posY - dialCos ((dialSize * tail) >> 6, angle + SCALE_2);
		points[2] = posX + dialSin (dialSize >> 6, angle + SCALE_1);
		points[3] = posY - dialCos (dialSize >> 6, angle + SCALE_1);
		points[4] = posX + dialSin ((dialSize * size) >> 6, angle);
		points[5] = posY - dialCos ((dialSize * size) >> 6, angle);
		points[6] = posX + dialSin (dialSize >> 6, angle + SCALE_3);
		points[7] = posY - dialCos (dialSize >> 6, angle + SCALE_3); 
		numPoints = 4;
		fill = 1;
		break;

	case 1:
		// Single triangle
		points[0] = posX + dialSin ((dialSize * tail) >> 6, angle + SCALE_2) 
					+ dialSin (dialSize >> 6, angle + SCALE_1);
		points[1] = posY - dialCos ((dialSize * tail) >> 6, angle + SCALE_2)
					- dialCos (dialSize >> 6, angle + SCALE_1);
		points[2] = posX + dialSin ((dialSize * tail) >> 6, angle + SCALE_2) 
					+ dialSin (dialSize >> 6, angle + SCALE_3);
		points[3] = posY - dialCos ((dialSize * tail) >> 6, angle + SCALE_2)
					- dialCos (dialSize >> 6, angle + SCALE_3);
		points[4] = posX + dialSin ((dialSize * size) >> 6, angle);
		points[5] = posY - dialCos ((dialSize * size) >> 6, angle);
		numPoints = 3;
		fill = 1;
		break;

	case 2:
		// Rectangle
		points[0] = posX + dialSin ((dialSize * tail) >> 6, angle + SCALE_2) + dialSin (dialSize >> 6, angle + SCALE_1);
		points[1] = posY - dialCos ((dialSize * tail) >> 6, angle + SCALE_2) - dialCos (dialSize >> 6, angle + SCALE_1);
		points[2] = posX + dialSin ((dialSize * tail) >> 6, angle + SCALE_2) + dialSin (dialSize >> 6, angle + SCALE_3);
		points[3] = posY - dialCos ((dialSize * tail) >> 6, angle + SCALE_2) - dialCos (dialSize >> 6, angle + SCALE_3);
		points[4] = posX + dialSin ((dialSize * size) >> 6, angle) + dialSin (dialSize >> 6, angle + SCALE_3);
		points[5] = posY - dialCos ((dialSize * size) >> 6, angle) - dialCos (dialSize >> 6, angle + SCALE_3);
		points[6] = posX + dialSin ((dialSize * size) >> 6, angle) + dialSin (dialSize >> 6, angle + SCALE_1);
		points[7] = posY - dialCos ((dialSize * size) >> 6, angle) - dialCos (dialSize >> 6, angle + SCALE_1);
		numPoints = 4;
		fill = 1;
		break;

	case 3:
		//Rectangle with pointer
		points[0] = posX + dialSin ((dialSize * tail) >> 6, angle + SCALE_2) + dialSin (dialSize >> 6, angle + SCALE_1);
		points[1] = posY - dialCos ((dialSize * tail) >> 6, angle + SCALE_2) - dialCos (dialSize >> 6, angle + SCALE_1);
		points[2] = posX + dialSin ((dialSize * tail) >> 6, angle + SCALE_2) + dialSin (dialSize >> 6, angle + SCALE_3);
		points[3] = posY - dialCos ((dialSize * tail) >> 6, angle + SCALE_2) - dialCos (dialSize >> 6, angle + SCALE_3);
		points[4] = posX + dialSin ((dialSize * (size * 15)) >> 10, angle) + dialSin (dialSize >> 6, angle + SCALE_3);
		points[5] = posY - dialCos ((dialSize * (size * 15)) >> 10, angle) - dialCos (dialSize >> 6, angle + SCALE_3);
		points[6] = posX + dialSin ((dialSize * size) >> 6, angle);
		points[7] = posY - dialCos ((dialSize * size) >> 6, angle);
		points[8] = posX + dialSin ((dialSize * (size * 15)) >> 10, angle) + dialSin (dialSize >> 6, angle + SCALE_1);
		points[9] = posY - dialCos ((dialSize * (size * 15)) >> 10, angle) - dialCos (dialSize >> 6, angle + SCALE_1);
		numPoints = 5;
		fill = 1;
		break;

	case 4:
		//Rectangle with arrow
		points[0] = posX + dialSin ((dialSize * tail) >> 6, angle + SCALE_2) + dialSin (dialSize >> 6, angle + SCALE_1);
		points[1] = posY - dialCos ((dialSize * tail) >> 6, angle + SCALE_2) - dialCos (dialSize >> 6, angle + SCALE_1);
		points[2] = posX + dialSin ((dialSize * tail) >> 6, angle + SCALE_2) + dialSin (dialSize >> 6, angle + SCALE_3);
		points[3] = posY - dialCos ((dialSize * tail) >> 6, angle + SCALE_2) - dialCos (dialSize >> 6, angle + SCALE_3);
		points[4] = posX + dialSin ((dialSize * (size * 12)) >> 10, angle) + dialSin (dialSize >> 6, angle + SCALE_3);
		points[5] = posY - dialCos ((dialSize * (size * 12)) >> 10, angle) - dialCos (dialSize >> 6, angle + SCALE_3);
		points[6] = posX + dialSin ((dialSize * (size * 12)) >> 10, angle) + dialSin (dialSize / 30, angle + SCALE_3);
		points[7] = posY - dialCos ((dialSize * (size * 12)) >> 10, angle) - dialCos (dialSize / 30, angle + SCALE_3);
		points[8] = posX + dialSin ((dialSize * size) >> 6, angle);
		points[9] = posY - dialCos ((dialSize * size) >> 6, angle);
		points[10] = posX + dialSin ((dialSize * (size * 12)) >> 10, angle) + dialSin (dialSize / 30, angle + SCALE_1);
		points[11] = posY - dialCos ((dialSize * (size * 12)) >> 10, angle) - dialCos (dialSize / 30, angle + SCALE_1);
		points[12] = posX + dialSin ((dialSize * (size * 12)) >> 10, angle) + dialSin (dialSize >> 6, angle + SCALE_1);
		points[13] = posY - dialCos ((dialSize * (size * 12)) >> 10, angle) - dialCos (dialSize >> 6, angle + SCALE_1);
		numPoints = 7;
		fill = 1;
		break;

	case 5:
		// Single triangle
		points[0] = posX + dialSin ((dialSize * tail) >> 6, angle + SCALE_2) + dialSin (dialSize / 40, angle + SCALE_1);
		points[1] = posY - dialCos ((dialSize * tail) >> 6, angle + SCALE_2) - dialCos (dialSize / 40, angle + SCALE_1);
		points[2] = posX + dialSin ((dialSize * tail) >> 6, angle + SCALE_2) + dialSin (dialSize / 40, angle + SCALE_3);
		points[3] = posY - dialCos ((dialSize * tail) >> 6, angle + SCALE_2) - dialCos (dialSize / 40, angle + SCALE_3);
		points[4] = posX + dialSin ((dialSize * size) >> 6, angle);
		points[5] = posY - dialCos ((dialSize * size) >> 6, angle);
		numPoints = 3;
		fill = 1;
		break;

	case 9:
	default:
		// Simple line
		points[0] = posX + dialSin ((dialSize * tail) >> 6, angle + SCALE_2);
		points[1] = posY - dialCos ((dialSize * tail) >> 6, angle + SCALE_2);
		points[2] = posX + dialSin ((dialSize * size) >> 6, angle);
		points[3] = posY - dialCos ((dialSize * size) >> 6, angle);
		numPoints = 2;
		break;
	}

	cairo_set_line_width (saveCairo, 1.0f + ((float)dialSize / 256.0f));
	if (!handStyle -> fillIn)
	{
		fill = 0;
	}
	for (i = 0; i < 2; i++)
	{
		if (i != 0 || fill)
		{
			dialSetColour (i == 0 ? colFill : colOut);
			cairo_move_to (saveCairo, points[0], points[1]);
			for (j = 1; j < numPoints; j++)
				cairo_line_to (saveCairo, points[j << 1], points[(j << 1) + 1]);
			if (numPoints > 2)
				cairo_close_path (saveCairo);
			if (i == 0)
				cairo_fill (saveCairo);
			cairo_stroke (saveCairo);
		}
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  D R A W  M A R K                                                                                         *
 *  =========================                                                                                         *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Draw a shaped marker on the dial.
 *  @param angle Angle of the marker.
 *  @param size Size of the marker.
 *  @param colFill Fill colour.
 *  @param colOut Outline colour.
 *  @param text Text to display on the mark.
 *  @result None.
 */
void dialDrawMark (int angle, int size, int colFill, int colOut, char *text)
{
	dialDrawMarkX (saveCentreX, saveCentreY, angle, size, colFill, colOut, text);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  D R A W  M A R K  X                                                                                      *
 *  ============================                                                                                      *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Draw a shaped marker on the dial.
 *  @param posX Centre position.
 *  @param posY Centre position.
 *  @param angle Angle of the marker.
 *  @param size Size of the marker.
 *  @param colFill Fill colour.
 *  @param colOut Outline colour.
 *  @param text Text to display on the mark.
 *  @result None.
 */
void dialDrawMarkX (int posX, int posY, int angle, int size, int colFill, int colOut, char *text)
{
	if (markerStep == 0) markerStep = SCALE_4;

	if (!(angle % markerStep))
	{
		switch (markerType)
		{
		case 0:
			/* No markers */
			break;
	
		case 1:
			/* Triangle markers */
			{
				int points[6], i;
				cairo_set_line_width (saveCairo, 1.0f + ((float)dialSize / 512.0f));
				points[0] = posX + dialSin ((dialSize * size) >> 6, angle + 5);
				points[1] = posY - dialCos ((dialSize * size) >> 6, angle + 5);
				points[2] = posX + dialSin ((dialSize * (size - 3)) >> 6, angle);
				points[3] = posY - dialCos ((dialSize * (size - 3)) >> 6, angle);
				points[4] = posX + dialSin ((dialSize * size) >> 6, angle - 5);
				points[5] = posY - dialCos ((dialSize * size) >> 6, angle - 5);
				for (i = 0; i < 2; i++)
				{
					dialSetColour (i == 0 ? colFill : colOut);
					cairo_move_to (saveCairo, points[0], points[1]);
					cairo_line_to (saveCairo, points[2], points[3]);
					cairo_line_to (saveCairo, points[4], points[5]);
					cairo_close_path (saveCairo);
					if (i == 0)	cairo_fill (saveCairo);
					cairo_stroke (saveCairo);
				}
			}
			break;

		case 2:
			/* Circle markers */
			cairo_set_line_width (saveCairo, 1.0f + ((float)dialSize / 512.0f));
			posX += dialSin ((dialSize * (size - 2)) >> 6, angle);
			posY -= dialCos ((dialSize * (size - 2)) >> 6, angle);
			dialDrawCircleX (posX, posY, 3, colFill, colOut);
			break;

		default:
			/* Text number markers */
			posX += dialSin ((dialSize * (size - 5)) >> 6, angle);
			posY -= dialCos ((dialSize * (size - 5)) >> 6, angle);
			dialDrawTextX (posX, posY, text, colOut, 1);
			break;
		}
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  G E T  F O N T  S I Z E                                                                                  *
 *  ================================                                                                                  *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Get the font size from a font def.
 *  @param fontName String to read the size from.
 *  @result The font size.
 */
float dialGetFontSize (char *fontName)
{
	int i = strlen (fontName);
	float size = 0;
	
	while (i)
	{
		i --;
		if ((fontName[i] >= '0' && fontName[i] <= '9') || fontName[i] == '.')
			continue;
		break;
	}
	if (i < strlen (fontName))
	{
		size = atof (&fontName[i + 1]);
	}
	return size;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  D R A W  T E X T                                                                                         *
 *  =========================                                                                                         *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Draw text on the dial.
 *  @param posn Top or Bottom of the dial.
 *  @param string1 Text to display.
 *  @param colour Text colour.
 *  @result None.
 */
void dialDrawText (int posn, char *string1, int colour)
{
	if (string1[0])
	{
		int posY;
		
		if (posn == 0)
			posY = savePosY + ((dialSize * 5) >> 4);
		else
			posY = savePosY + ((dialSize * 11) >> 4);

		dialDrawTextX (saveCentreX, posY, string1, colour, 0);
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  D R A W  T E X T  X                                                                                      *
 *  ============================                                                                                      *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Draw text on the dial.
 *  @param posX Centre position.
 *  @param posY Centre position.
 *  @param string1 Text to display.
 *  @param colour Text colour.
 *  @param scale Scale factor.
 *  @result None.
 */
void dialDrawTextX (int posX, int posY, char *string1, int colour, int scale)
{
	if (string1[0])
	{
		int fontSize = 0;
		gint posW, posH;

		if ((fontSize = dialGetFontSize (fontName)) == 0)
		{
			fontSize = (dialSize >> 6) << 2;
			if (fontSize < 6) fontSize = 6;
		}
		if (scale)
			fontSize = (fontSize * 10) / 12;

		PangoLayout *layout = pango_cairo_create_layout (saveCairo);
		PangoFontDescription *fontDesc = pango_font_description_from_string (fontName);

		pango_font_description_set_size (fontDesc, PANGO_SCALE * fontSize);
 		pango_layout_set_font_description (layout, fontDesc); 
		pango_layout_set_alignment (layout, PANGO_ALIGN_CENTER);

		dialSetColour (colour);
		pango_layout_set_text (layout, string1, -1);
		pango_layout_get_pixel_size (layout, &posW, &posH);

		cairo_move_to (saveCairo, posX - (posW >> 1), posY - (posH >> 1));
		pango_cairo_show_layout (saveCairo, layout);

		pango_font_description_free (fontDesc);
		g_object_unref (layout);
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  F O N T  C A L L B A C K                                                                                 *
 *  =================================                                                                                 *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Function to handle changes to the fonts.
 *  @param data Not used.
 *  @result None.
 */
void
dialFontCallback (guint data)
{
	GtkWidget *dialog;
	gchar *selectedFont;
	bool reRun = false;

#if GTK_MAJOR_VERSION == 2
	dialog = gtk_font_selection_dialog_new ("Pick the gauge font");
	gtk_font_selection_dialog_set_font_name ((GtkFontSelectionDialog *)dialog, fontName);
#else
	dialog = gtk_font_chooser_dialog_new (_("Pick the gauge font"), mainWindow);
	gtk_font_chooser_set_font ((GtkFontChooser *)dialog, fontName);
#endif

	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
	gtk_widget_show_all (dialog);
	do
	{
		reRun = false;

		switch (gtk_dialog_run (GTK_DIALOG (dialog)))
		{
		case GTK_RESPONSE_CANCEL:
			break;
		case GTK_RESPONSE_APPLY:
			reRun = true;
		default:
#if GTK_MAJOR_VERSION == 2
			selectedFont = gtk_font_selection_dialog_get_font_name ((GtkFontSelectionDialog *)dialog);
#else
			selectedFont = gtk_font_chooser_get_font ((GtkFontChooser *)dialog);
#endif
			strcpy (fontName, selectedFont);
			g_free (selectedFont);
			configSetValue ("font_name", fontName);
			if (UpdateFunc) UpdateFunc();
			break;
		}
	}
	while (reRun);

	gtk_widget_destroy (dialog);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  C O L O U R  C O M B O  C A L L B A C K                                                                  *
 *  ================================================                                                                  *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Drop down to pick a colour to change.
 *  @param comboBox Combobox for the list.
 *  @param data Not sure.
 *  @result None.
 */
void dialColourComboCallback (GtkWidget *comboBox, gpointer data)
{
	GtkWidget *colourSel = data;
	int i = gtk_combo_box_get_active (GTK_COMBO_BOX (comboBox)) + 2;

	if (i >= 2 && i <= 23)
	{
#if GTK_MAJOR_VERSION == 2
		gtk_color_selection_set_current_color (GTK_COLOR_SELECTION (colourSel), dialColour(i));
#else
		gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (colourSel), dialColour(i));
#endif
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  C O L O U R  C A L L B A C K                                                                             *
 *  =====================================                                                                             *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Display a dialog to pick the colour to change.
 *  @param data Not used.
 *  @result None.
 */
void
dialColourCallback (guint data)
{
	int i;
	bool reRun = false;
	GtkWidget *vbox;
	GtkWidget *dialog;
	GtkWidget *comboBox;
	GtkWidget *colourSel;
#if GTK_MAJOR_VERSION == 2
	GdkColor setColour;
#else
	GtkWidget *contentArea;
	GdkRGBA setColour;
#endif

	dialog = gtk_dialog_new_with_buttons (_("Pick the clock colour"), mainWindow,
		    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, 
#if GTK_MAJOR_VERSION == 3 && GTK_MINOR_VERSION >= 10
			_("Apply"), GTK_RESPONSE_APPLY,
			_("Close"), GTK_RESPONSE_ACCEPT, 
#else
			GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
			GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT, 
#endif
			NULL);

#if GTK_MAJOR_VERSION == 2
	vbox = GTK_DIALOG (dialog)->vbox;
	comboBox = gtk_combo_box_new_text ();
	for (i = 2; i < dialMaxColours; i++)
		gtk_combo_box_append_text (GTK_COMBO_BOX (comboBox), dialColours[i].longName);
#else
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
	contentArea = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_box_pack_start (GTK_BOX (contentArea), vbox, FALSE, TRUE, 0);
	comboBox = gtk_combo_box_text_new ();
	for (i = 2; i < dialMaxColours; i++)
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (comboBox), dialColours[i].longName);
#endif

	gtk_combo_box_set_active (GTK_COMBO_BOX (comboBox), 0);

#if GTK_MAJOR_VERSION == 2
	colourSel = gtk_color_selection_new ();
	gtk_color_selection_set_has_opacity_control (GTK_COLOR_SELECTION (colourSel), false);
	gtk_color_selection_set_current_color (GTK_COLOR_SELECTION (colourSel), &dialColours[2].dialColour);
#else
	colourSel = gtk_color_chooser_widget_new ();
	gtk_color_chooser_set_use_alpha (GTK_COLOR_CHOOSER (colourSel), true); 
	gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (colourSel), &dialColours[2].dialColour);
#endif
	g_signal_connect (comboBox, "changed", G_CALLBACK (dialColourComboCallback), colourSel);

	gtk_container_add (GTK_CONTAINER (vbox), comboBox);
	gtk_container_add (GTK_CONTAINER (vbox), colourSel);
	gtk_widget_show_all (dialog);

	do
	{
		reRun = false;

		switch (gtk_dialog_run (GTK_DIALOG (dialog)))
		{
		case GTK_RESPONSE_APPLY:
			reRun = true;
		default:
			i = gtk_combo_box_get_active (GTK_COMBO_BOX (comboBox)) + 2;
			if (i >= 2 && i <= 23)
			{
				gchar *colString = NULL;
#if GTK_MAJOR_VERSION == 2
				gtk_color_selection_get_current_color (GTK_COLOR_SELECTION (colourSel), &setColour);
	#if GTK_MINOR_VERSION > 11
				colString = gdk_color_to_string (&setColour);
	#else
				colString = g_malloc (10);
				sprintf (colString, "#%02X%02X%02X", setColour.red / 256, setColour.green / 256, 
						setColour.blue / 256);
	#endif
#else
				gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (colourSel), &setColour);
				colString = gdk_rgba_to_string (&setColour);
#endif
				if (colString)
				{
					strncpy (dialColours[i].defColour, colString, 60);
					g_free (colString);
				}
				dialColours[i].dialColour = setColour;
				if (UpdateFunc) UpdateFunc();
			}
			break;
		}
	}
	while (reRun);

	gtk_widget_destroy (comboBox);
	gtk_widget_destroy (colourSel);
	gtk_widget_destroy (dialog); 
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  C R E A T E  C O L O U R S                                                                               *
 *  ===================================                                                                               *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Convert the config colours into real colours.
 *  @result None.
 */
int
dialCreateColours ()
{
	int i = 0;
#if GTK_MAJOR_VERSION == 2
	colourMap = gdk_colormap_get_system ();
#endif

	while (dialColours[i].shortName)
	{
#if GTK_MAJOR_VERSION == 2
		gdk_color_parse (dialColours[i].defColour, &dialColours[i].dialColour);
		gdk_colormap_alloc_color (colourMap, &dialColours[i].dialColour, FALSE, FALSE);
#else
		gdk_rgba_parse (&dialColours[i].dialColour, dialColours[i].defColour);
#endif
		++i;
	}
	return i;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  F I L L  S I N  C O S  T A B L E S                                                                       *
 *  ===========================================                                                                       *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Fill the sin and cos tables.
 *  @param start Starting position .
 *  @result None.
 */
void dialFillSinCosTables (int start)
{
	int i, x = start;
	
	for (i = 0; i < SCALE_4; i++)
	{
		sinTable[i] = sin (((double) x * M_PI) / SCALE_2);
		cosTable[i] = cos (((double) x * M_PI) / SCALE_2);
		if (++x == SCALE_4)
			x = 0;
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  S I N  C O S                                                                                             *
 *  =====================                                                                                             *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Get the sin value from the table.
 *  @param number Not sure.
 *  @param angle Angle to read.
 *  @param useCos Use COS or SIN table.
 *  @result Value.
 */
int dialSin (int number, int angle)
{
	while (angle < 0) angle += SCALE_4;
	angle %= SCALE_4;
	
	return (int)rint(number * sinTable[angle]);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  S I N  C O S                                                                                             *
 *  =====================                                                                                             *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Get the cos value from the table.
 *  @param number Not sure.
 *  @param angle Angle to read.
 *  @param useCos Use COS or SIN table.
 *  @result Value.
 */
int dialCos (int number, int angle)
{
	while (angle < 0) angle += SCALE_4;
	angle %= SCALE_4;
	
	return (int)rint(number * cosTable[angle]);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  F I X  F A C E  S I Z E                                                                                  *
 *  ================================                                                                                  *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Check the new face size is OK.
 *  @result None.
 */
void dialFixFaceSize (void)
{
	int max = 1024;
	
	if (max * dialWidth > gdk_screen_width())
	{
		max = ((gdk_screen_width() / dialWidth) * 64) / 64;
	}
	if (max * dialHeight > gdk_screen_height())
	{
		max = ((gdk_screen_height() / dialHeight) * 64) / 64;
	}
	if (dialSize > max)
	{
		dialSize = max;
	}
	else if (dialSize < 64)
	{
		dialSize = 64;
	}
	else
	{
		dialSize = ((dialSize + 32) / 64) * 64;
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  Z O O M  C A L L B A C K                                                                                 *
 *  =================================                                                                                 *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Zoom in or out of the dial.
 *  @param data In or out.
 *  @result None.
 */
void
dialZoomCallback (guint data)
{
	switch (data)
	{
	case 1:
		dialSize += 64;
		break;
	case 2:
		dialSize -= 64;
		break;
	case 3:
		dialSize = 1024;
		break;
	}	
	dialFixFaceSize ();
	configSetIntValue ("face_size", dialSize);
	
	dialWindowMask();
	gtk_widget_set_size_request (drawingArea, dialWidth * dialSize, dialHeight * dialSize);
	
	if (UpdateFunc) UpdateFunc();
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  M A R K E R  C A L L B A C K                                                                             *
 *  =====================================                                                                             *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Select the type of marker to use.
 *  @param data Any data.
 *  @result None.
 */
void
dialMarkerCallback (guint data)
{
	markerType = data;
	configSetIntValue ("marker_type", markerType);
	if (UpdateFunc) UpdateFunc();
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  S T E P  C A L L B A C K                                                                                 *
 *  =================================                                                                                 *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief How often to draw the hour markers.
 *  @param data Change the step value.
 *  @result None.
 */
void
dialStepCallback (guint data)
{
	markerStep = data;
	configSetIntValue ("marker_step", markerStep);
	if (UpdateFunc) UpdateFunc();
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  A D D  D E L  C A L L B A C K                                                                            *
 *  ======================================                                                                            *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Add or delete a column or row.
 *  @param data What to do.
 *  @result None.
 */
void
dialAddDelCallback (guint data)
{
	switch (data)
	{
	case 1:
		if (dialWidth < 10 && ((dialWidth + 1) * dialHeight) <= MAX_FACES)
			dialWidth ++;
		break;
	case 2:
		if (dialWidth > 1)
			dialWidth --;
		break;
	case 3:
		if (dialHeight < 10 && ((dialHeight + 1) * dialWidth) <= MAX_FACES)
			dialHeight ++;
		break;
	case 4:
		if (dialHeight > 1)
			dialHeight --;
		break;
	}
	configSetIntValue ("number_cols", dialWidth);
	configSetIntValue ("number_rows", dialHeight);

	dialWindowMask();
	gtk_widget_set_size_request (drawingArea, dialWidth * dialSize, dialHeight * dialSize);

	if (UpdateFunc) UpdateFunc();
}	

/**********************************************************************************************************************
 *                                                                                                                    *
 *  D I A L  S A V E  C A L L B A C K                                                                                 *
 *  =================================                                                                                 *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Called to save an SVG copy of the face (-V to enable).
 *  @param data Not used.
 *  @result None.
 */
void dialSaveCallback (guint data)
{
	if (DialSave)
	{
		GtkWidget *dialog;

		dialog = gtk_file_chooser_dialog_new ("Save file",
				mainWindow,
				GTK_FILE_CHOOSER_ACTION_SAVE,
#if GTK_MAJOR_VERSION == 3 && GTK_MINOR_VERSION >= 10
				_("Save"), GTK_RESPONSE_ACCEPT,
				_("Cancel"), GTK_RESPONSE_CANCEL, 
#else
				GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, 
#endif
				NULL);
		gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
	
		if (saveFilePath[0])
			gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (dialog), saveFilePath);
		else
			gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), "dial.svg");

		if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
		{
			char *filename;
	
			filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
			strcpy (saveFilePath, filename);
			DialSave (saveFilePath);
			g_free (filename);
		}
		gtk_widget_destroy (dialog);
	}
}
