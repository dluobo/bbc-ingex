#ifndef __MOUSE_INPUT_H__
#define __MOUSE_INPUT_H__



#ifdef __cplusplus
extern "C" 
{
#endif


typedef struct
{
    void* data; /* passed to functions */
    
    void (*click)(void* data, int imageWidth, int imageHeight, int xPos, int yPos);
} MouseInputListener;

typedef struct
{
    void* data; /* passed to functions */
    
    void (*set_listener)(void* data, MouseInputListener* listener);
    void (*unset_listener)(void* data);
    void (*close)(void* data);
} MouseInput;


/* utility functions for calling MouseInputListener functions */

void mil_click(MouseInputListener* listener, int imageWidth, int imageHeight, int xPos, int yPos);


/* utility functions for calling MouseInput functions */

void mip_set_listener(MouseInput* input, MouseInputListener* listener);
void mip_unset_listener(MouseInput* input);
void mip_close(MouseInput* input);




#ifdef __cplusplus
}
#endif



#endif


