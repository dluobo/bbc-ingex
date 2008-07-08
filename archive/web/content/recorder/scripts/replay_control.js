var reverseSpeed = [1, 2, 5, 10, 20, 50, 100]; 
var forwardSpeed = [2, 5, 10, 20, 50, 75, 100]; 

var button10Pressed = false;
var prevMarkDownTime = null;
var nextMarkDownTime = null;

var escapeKeyCode = 135; // F24
var numlockKeyCode = 144;
// button 10, 14 and 15 use single keystrokes without escape so that a keydown and keyup with wait 
// in-between can be detected
var button10KeyCode = 125; // F14
var button14KeyCode = 126; // F15
var button15KeyCode = 127; // F16

var lastKeyCodeUp = null;



function send_replay_command(cmd)
{
    // cmd must already by URI escaped
    var replayCommandRequest = new XMLHttpRequest();
    replayCommandRequest.open("GET", "/confreplay/" + cmd, true);
    replayCommandRequest.send(null);
}

function seek_home()
{
    send_replay_command("home?pause=true");
}

function seek_end()
{
    send_replay_command("end?pause=true")
}

function fast_rewind_factor(factor)
{
    send_replay_command("play-speed-factor?factor=-" + factor)
}

function fast_forward_factor(factor)
{
    send_replay_command("play-speed-factor?factor=" + factor)
}

function play()
{
    send_replay_command("play");
}

function pause()
{
    send_replay_command("pause");
}

function seek_prev_mark()
{
    send_replay_command("prev-mark");
}

function seek_next_mark()
{
    send_replay_command("next-mark");
}

function step_forwards()
{
    send_replay_command("step?forward=true&unit=FRAME_PLAY_UNIT&pause=true");
}

function step_backwards()
{
    send_replay_command("step?forward=false&unit=FRAME_PLAY_UNIT&pause=true");
}

function seek_10p_forwards()
{
    send_replay_command("seek?offset=10000&whence=SEEK_CUR&unit=PERCENTAGE_PLAY_UNIT&pause=true");
}

function seek_10p_backwards()
{
    send_replay_command("seek?offset=-10000&whence=SEEK_CUR&unit=PERCENTAGE_PLAY_UNIT&pause=true");
}

function stop()
{
    send_replay_command("stop");
}

function next_osd_screen()
{
    send_replay_command("next-osd-screen");
}

function next_osd_timecode()
{
    send_replay_command("next-osd-timecode");
}

function review()
{
    send_replay_command('review?duration=250');    
}

function step_perc_forwards()
{
    send_replay_command("step?forward=true&unit=PERCENTAGE_PLAY_UNIT");
}

function step_perc_backwards()
{
    send_replay_command("step?forward=false&unit=PERCENTAGE_PLAY_UNIT");
}

function fast_forward(speed)
{
    send_replay_command("play-speed?speed=" + speed + "&unit=FRAME_PLAY_UNIT");
}

function fast_rewind(speed)
{
    send_replay_command("play-speed?speed=-" + speed + "&unit=FRAME_PLAY_UNIT");
}

function seek_percentage(perc)
{
    send_replay_command("seek?offset=" + parseInt(perc * 1000) + "&whence=SEEK_SET&unit=PERCENTAGE_PLAY_UNIT&pause=true");
}

function toggle_marks_bar()
{
    send_replay_command("next-marks-selection");
}


// override this function
function set_user_mark()
{
    // default is to do nothing
}

// override this function
function remove_user_mark()
{
    // default is to do nothing
}

function handle_replay_key_down(event)
{
    // ignore the numlock key events which are generate by the shuttle (don't know why)
    if (event.keyCode == numlockKeyCode)
    {
        return;
    }

    // ignore keys not preceded by the escape key, except button 14 and 15 key codes, or the escape key
    if (event.keyCode == escapeKeyCode || 
        (lastKeyCodeUp != escapeKeyCode && event.keyCode != button10KeyCode && event.keyCode != button14KeyCode && 
            event.keyCode != button15KeyCode))
    {
        return;
    }

    var speed;
    switch (event.keyCode)
    {
        case 49: // button 1
            next_osd_screen();
            break;
            
        case 50: // button 2
            next_osd_timecode();
            break;
            
        case 51: // button 3
            toggle_marks_bar();
            break;
            
        case 53: // button 5
            set_user_mark();
            break;
            
        case 54: // button 6
            remove_user_mark();
            break;
            
        case 55: // button 7
            play();
            break;
            
        case 56: // button 8
            pause();
            break;
            
        case 57: // button 9
            set_user_mark();
            break;
            
        case button10KeyCode:
            button10Pressed = true;
            break;
            
        case button14KeyCode: // button 14
            // wait until key up event
            prevMarkDownTime = new Date();
            break;
            
        case button15KeyCode: // button 15
            // wait until key up event
            nextMarkDownTime = new Date();
            break;

        case 71: // jog right -> step forwards
            if (button10Pressed)
            {
                step_perc_forwards();
            }
            else
            {
                step_forwards();
            }
            break;
            
        case 72: // jog left -> step backwards
            if (button10Pressed)
            {
                step_perc_backwards();
            }
            else
            {
                step_backwards();
            }
            break;
            
        case 73: // shuttle right 1 - forward speed 1
        case 74: // shuttle right 2 - forward speed 2
        case 75: // shuttle right 3 - forward speed 3
        case 76: // shuttle right 4 - forward speed 4
        case 77: // shuttle right 5 - forward speed 5
        case 78: // shuttle right 6 - forward speed 6
        case 79: // shuttle right 7 - forward speed 7
            speed = forwardSpeed[event.keyCode - 73];
            fast_forward(speed);
            break;
            
        case 80: // shuttle left 1 - rewind speed 1
        case 81: // shuttle left 2 - rewind speed 2
        case 82: // shuttle left 3 - rewind speed 3
        case 83: // shuttle left 4 - rewind speed 4
        case 84: // shuttle left 5 - rewind speed 5
        case 85: // shuttle left 6 - rewind speed 6
        case 86: // shuttle left 7 - rewind speed 7
            speed = reverseSpeed[event.keyCode - 80];
            fast_rewind(speed);
            break;
            
        case 87: // shuttle centre - pause
            pause();
            break;
    }
}

function handle_replay_key_up(event)
{
    // ignore the numlock key events which are generate by the shuttle (don't know why)
    if (event.keyCode == numlockKeyCode)
    {
        return;
    }
    
    lastKeyCodeUp = event.keyCode;
    
    // ignore escape key code events
    if (event.keyCode == escapeKeyCode)
    {
        return;
    }
    
    var prevMarkUpTime;
    var nextMarkUpTime;
    
    switch (event.keyCode)
    {
        case button10KeyCode:
            button10Pressed = false;
            break;
            
        case button14KeyCode: // button 14
            prevMarkUpTime = new Date();
            if (prevMarkDownTime != null && prevMarkUpTime.getTime() - prevMarkDownTime.getTime() > 1000)
            {
                seek_home();
            }
            else
            {
                seek_prev_mark();
            }
            break;
            
        case button15KeyCode: // button 15
            nextMarkUpTime = new Date();
            if (nextMarkDownTime != null && nextMarkUpTime.getTime() - nextMarkDownTime.getTime() > 1000)
            {
                seek_end();
            }
            else
            {
                seek_next_mark();
            }
            break;
    }
}

function filter_replay_controls(event)
{
    if (event.which == escapeKeyCode || event.which == button14KeyCode || event.which == button15KeyCode ||
        event.which == button10KeyCode || lastKeyCodeUp == escapeKeyCode)
    {
        // shuttle control key
        return false;
    }
    
    return true;
}

