/**********************************************************************************************************************
 *                                                                                                                    *
 *  G A U G E  T H E R M O . C                                                                                        *
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
 *  @brief Routines to display the thermometer information.
 *  @version $Id: GaugeWeather.c 1531 2012-07-31 12:08:36Z ukchkn $
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <dirent.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "config.h"
#include "socket.h"
#include "GaugeDisp.h"

#ifdef GAUGE_HAS_THERMO

extern FACE_SETTINGS *faceSettings[];
extern MENU_DESC gaugeMenuDesc[];
extern char thermoServer[];
extern int thermoPort;

double myThermoReading[5] = { 0, 0, 0, 0, 0 };

/**********************************************************************************************************************
 *                                                                                                                    *
 *  R E A D  T H E R M O M E T E R  I N I T                                                                           *
 *  =======================================                                                                           *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Called once at the program start to find the devices.
 *  @result None 0 all is OK.
 */
void readThermometerInit (void)
{
	char addr[20];
	int clientSock = -1;
	
	if (GetAddressFromName (thermoServer, addr))
	{
		clientSock = ConnectClientSocket (addr, thermoPort);
	}
	if (SocketValid (clientSock))
	{
		gaugeMenuDesc[MENU_GAUGE_THERMO].disable = 0;
		CloseSocket (&clientSock);
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P R O C E S S  P O W E R  K E Y                                                                                   *
 *  ===============================                                                                                   *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Process each of the fields in the XML.
 *  @param readLevel 0 current, 1 today, 2 tomorrow.
 *  @param name Name of the field.
 *  @param value Value of the field.
 *  @result None.
 */
static void processThermoKey (int readLevel, const char *name, char *value)
{
	if (readLevel == 1 && strcmp (name, "outside") == 0)
		myThermoReading[0] = atof (value);
	if (readLevel == 1 && strcmp (name, "inside") == 0)
		myThermoReading[1] = atof (value);
	if (readLevel == 1 && strcmp (name, "pressure") == 0)
		myThermoReading[2] = atof (value);
	if (readLevel == 1 && strcmp (name, "light") == 0)
		myThermoReading[3] = atof (value);
	if (readLevel == 1 && strcmp (name, "humidity") == 0)
		myThermoReading[4] = atof (value);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P R O C E S S  E L E M E N T  N A M E S                                                                           *
 *  =======================================                                                                           *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Process each of the elements in the file.
 *  @param doc Document to read.
 *  @param aNode Current node.
 *  @param readLevel 0 current, 1 today, 2 tomorrow.
 *  @result None.
 */
static void processElementNames (xmlDoc *doc, xmlNode * aNode, int readLevel)
{
	xmlChar *key;
    xmlNode *curNode = NULL;

    for (curNode = aNode; curNode; curNode = curNode->next) 
    {
        if (curNode->type == XML_ELEMENT_NODE) 
        {
			if ((!xmlStrcmp (curNode -> name, (const xmlChar *)"sensors"))) 
			{
				++readLevel;
			}
			else
			{
				key = xmlNodeListGetString (doc, curNode -> xmlChildrenNode, 1);
				processThermoKey (readLevel, (const char *)curNode -> name, (char *)key);
		    	xmlFree (key);
		    }
        }
        processElementNames (doc, curNode->children, readLevel);
    }
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P R O C E S S  B U F F E R                                                                                        *
 *  ==========================                                                                                        *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Process the down loaded buffer.
 *  @param buffer Buffer to process.
 *  @param size Size of the buffer.
 *  @result None.
 */
static void processBuffer (char *buffer, size_t size)
{
	xmlDoc *doc = NULL;
	xmlNode *rootElement = NULL;
	xmlChar *xmlBuffer = NULL;

	xmlBuffer = xmlCharStrndup (buffer, size);
	doc = xmlParseDoc (xmlBuffer);

	if (doc != NULL)
	{
		rootElement = xmlDocGetRootElement (doc);
		processElementNames (doc, rootElement, 0);
		xmlFreeDoc (doc);
	}
	else
	{
		printf ("error: could not parse memory\n");
		printf ("BUFF[%d] [[[%s]]]\n", size, buffer);
	}
	xmlCleanupParser ();
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  R E A D  T H E R M O M E T E R  I N F O                                                                           *
 *  =======================================                                                                           *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Read the current tempature from the thermometer.
 *  @result None.
 */
void readThermometerInfo ()
{
	char buffer[512] = "", addr[20];
	int clientSock = -1, bytesRead = 0;
	
	if (GetAddressFromName (thermoServer, addr))
	{
		clientSock = ConnectClientSocket (addr, thermoPort);
	}
	if (SocketValid (clientSock))
	{
		bytesRead = RecvSocket (clientSock, buffer, 511);
		CloseSocket (&clientSock);
	}
	if (bytesRead)
	{
		processBuffer (buffer, bytesRead);
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  R E A D  T H E R M O M E T E R  V A L U E S                                                                       *
 *  ===========================================                                                                       *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  @brief Calculate the value to show on the face.
 *  @param face Which face to display on.
 *  @result None zero if the face should be re-shown.
 */
void readThermometerValues (int face)
{
	FACE_SETTINGS *faceSetting = faceSettings[face];

	if (faceSetting -> faceFlags & FACE_REDRAW)
	{
		;
	}
	else if (faceSetting -> nextUpdate)
	{
		faceSetting -> nextUpdate -= 1;
		return;
	}
	else if (!faceSetting -> nextUpdate)
	{
		readThermometerInfo ();
		faceSetting -> nextUpdate = 60;
	}

	setFaceString (faceSetting, FACESTR_TOP, 0, "Thermometer");
	setFaceString (faceSetting, FACESTR_TIP, 0, _("<b>Outside</b>: %0.1f\302\260C\n<b>Inside</b>: %0.1f\302\260C\n"
			"<b>Pressure</b>: %0.0fmb\n<b>Brightness</b>: %0.0flux\n<b>Humidity</b>: %0.0f%%"), 
			myThermoReading[0], myThermoReading[1], myThermoReading[2], myThermoReading[3], myThermoReading[4]);
	setFaceString (faceSetting, FACESTR_WIN, 0, _("Outside: %0.1f%s, Inside: %0.1f%s"),
			myThermoReading[0], "\302\260C", myThermoReading[1], "\302\260C");
	setFaceString (faceSetting, FACESTR_BOT, 0, "%0.1f%s", myThermoReading[0], "\302\260C");
	faceSetting -> firstValue = myThermoReading[0];
	faceSetting -> secondValue = myThermoReading[1];
}

#endif

