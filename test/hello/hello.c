// first OpenVG program
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "VG/openvg.h"
#include "VG/vgu.h"
#include "fontinfo.h"
#include "shapes.h"

int main() {
    int width, height;
    VGfloat w2, h2, w;
    char s[3];

    init(&width, &height);                                      // Graphics initialization

    w2 = (VGfloat)(width/2);
    h2 = (VGfloat)(height/2);
    w  = (VGfloat)w;

    Start(width, height);                                       // Start the picture
    Background(0, 0, 0);                                        // Black background
    Fill(44, 77, 232, 1);                                       // Big blue marble
    Circle(w2, 0, w);                                           // The "world"
    Fill(255, 255, 255, 1);                                     // White text
    TextMid(w2, h2, "hello, world", SerifTypeface, width/10);   // Greetings 
    End();                                                      // End the picture
    fgets(s, 2, stdin);                                         // Pause until RETURN]
    finish();                                                   // Graphics cleanup
    exit(0);
}

