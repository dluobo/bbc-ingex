#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#include "mouse_input.h"


void mil_click(MouseInputListener* listener, int imageWidth, int imageHeight, int xPos, int yPos)
{
    if (listener && listener->click)
    {
        listener->click(listener->data, imageWidth, imageHeight, xPos, yPos);
    }
}

void mip_set_listener(MouseInput* input, MouseInputListener* listener)
{
    if (input && input->set_listener)
    {
        input->set_listener(input->data, listener);
    }
}

void mip_unset_listener(MouseInput* input)
{
    if (input && input->unset_listener)
    {
        input->unset_listener(input->data);
    }
}

void mip_close(MouseInput* input)
{
    if (input && input->close)
    {
        input->close(input->data);
    }
}





