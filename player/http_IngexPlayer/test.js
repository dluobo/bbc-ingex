
var playerStateRequest = null;
var stateUpdateInterval = 1000;


function get_tag_state(tagE)
{
    if (tagE == null || !tagE.hasOwnProperty("ingexstate"))
    {
        return null;
    }
    
    return tagE.ingexstate;
}

function set_tag_state(tagE, state)
{
    if (tagE == null)
    {
        return;
    }
    
    return tagE.ingexstate = state;
}

function update_tag_value(id, value, stateValue)
{
    var tagE = document.getElementById(id);
    
    if (tagE != null)
    {
        if (get_tag_state(tagE) != stateValue)
        {
            tagE.innerHTML = value;
            set_tag_state(tagE, stateValue);
            return true;
        }
    }
    return false;
}

function update_tag_attr(id, attr, value)
{
    var tagE = document.getElementById(id);
    
    if (tagE != null)
    {
        if (tagE.getAttribute(attr) != value)
        {
            tagE.setAttribute(attr, value);
            return true;
        }
    }
    return false;
}

function update_tag_value_ele(tagE, value, stateValue)
{
    if (tagE != null)
    {
        if (get_tag_state(tagE) != stateValue)
        {
            tagE.innerHTML = value;
            set_tag_state(tagE, stateValue);
            return true;
        }
    }
    return false;
}

function update_tag_attr_ele(tagE, attr, value)
{
    if (tagE != null)
    {
        if (tagE.getAttribute(attr) != value)
        {
            tagE.setAttribute(attr, value);
            return true;
        }
    }
    return false;
}


function get_bool_state(value)
{
    if (value)
    {
        return "true";
    }
    else
    {
        return "false";
    }
}

function get_pos_state(pos)
{
    if (pos == null || pos < 0)
    {
        return "";
    }
    
    var posArray = new Array(4);
    posArray[0] = parseInt(pos / (60 * 60 * 25));
    posArray[0] = (posArray[0] < 10 ? "0" : "") + posArray[0];
    posArray[1] = parseInt((pos % (60 * 60 * 25)) / (60 * 25));
    posArray[1] = (posArray[1] < 10 ? "0" : "") + posArray[1];
    posArray[2] = parseInt(((pos % (60 * 60 * 25)) % (60 * 25)) / 25);
    posArray[2] = (posArray[2] < 10 ? "0" : "") + posArray[2];
    posArray[3] = ((pos % (60 * 60 * 25)) % (60 * 25)) % 25;
    posArray[3] = (posArray[3] < 10 ? "0" : "") + posArray[3];
    
    return posArray.join(":");
}

function get_timecode_state(tc)
{
    if (tc == null)
    {
        return "";
    }
    
    var tcArray = new Array(4);
    tcArray[0] = tc.hour;
    tcArray[1] = tc.min;
    tcArray[2] = tc.sec;
    tcArray[3] = tc.frame;
    
    if (tc.isDropFrame)
    {
        return tcArray.join(";");
    }
    else
    {
        return tcArray.join(":");
    }
}

function get_int_state(value)
{
    return value + "";
}

function set_player_state(state)
{
    var closedStateE = document.getElementById("closed-state");
    var lockedStateE = document.getElementById("locked-state");
    var playStateE = document.getElementById("play-state");
    var stopStateE = document.getElementById("stop-state");
    var speedStateE = document.getElementById("speed-state");
    var positionStateE = document.getElementById("position-state");
    var timecodeStateE = document.getElementById("timecode-state");
    
    if (state == null || 
        (state.hasOwnProperty("closed") && state.closed))
    {
        if (state == null)
        {
            update_tag_value_ele(closedStateE, "", "");
        }
        else
        {
            update_tag_value_ele(closedStateE, "true", "true");
        }
        
        update_tag_value_ele(lockedStateE, "", "");
        update_tag_value_ele(playStateE, "", "");
        update_tag_value_ele(stopStateE, "", "");
        update_tag_value_ele(speedStateE, "", "");
        update_tag_value_ele(positionStateE, "", "");
        update_tag_value_ele(timecodeStateE, "", "");
        
        return;
    }

    update_tag_value_ele(closedStateE, "false", "false");
    update_tag_value_ele(lockedStateE, get_bool_state(state.locked), get_bool_state(state.locked));
    update_tag_value_ele(playStateE, get_bool_state(state.play), get_bool_state(state.play));
    update_tag_value_ele(stopStateE, get_bool_state(state.stop), get_bool_state(state.stop));
    update_tag_value_ele(speedStateE, get_int_state(state.speed), get_int_state(state.speed));
    update_tag_value_ele(positionStateE, get_pos_state(state.displayedFrame.position), get_pos_state(state.displayedFrame.position));
    if (state.displayedFrame.hasOwnProperty("timecodes") && state.displayedFrame.timecodes.length > 0)
    {
        update_tag_value_ele(timecodeStateE, get_timecode_state(state.displayedFrame.timecodes[0]), get_timecode_state(state.displayedFrame.timecodes[0]));
    }
}

function player_state_handler()
{
    try
    {
        if (playerStateRequest.readyState == 4)
        {
            if (playerStateRequest.status != 200)
            {
                throw "request player state not ok";
            }
            
            var state = eval("(" + playerStateRequest.responseText + ")");
            
            set_player_state(state);
            
            stateUpdateTimer = setTimeout("request_player_state()", stateUpdateInterval)
        }
    }
    catch (err)
    {
        set_player_state(null);
        stateUpdateTimer = setTimeout("request_player_state()", stateUpdateInterval);
    }
}

function request_player_state()
{
    playerStateRequest = new XMLHttpRequest();
    playerStateRequest.open("GET", "/info/state.json", true);
    playerStateRequest.onreadystatechange = player_state_handler;
    playerStateRequest.send(null);
}



function set_dvs_output_type()
{
    var replayCommandRequest = new XMLHttpRequest();
    replayCommandRequest.open("GET", "/settings/outputtype?type=1", true);
    replayCommandRequest.send(null);
}

function set_dual_output_type()
{
    var replayCommandRequest = new XMLHttpRequest();
    replayCommandRequest.open("GET", "/settings/outputtype?type=5", true);
    replayCommandRequest.send(null);
}

function set_x11_output_type()
{
    var replayCommandRequest = new XMLHttpRequest();
    replayCommandRequest.open("GET", "/settings/outputtype?type=2", true);
    replayCommandRequest.send(null);
}


function send_command(cmd)
{
    // cmd must already by URI escaped
    var replayCommandRequest = new XMLHttpRequest();
    replayCommandRequest.open("GET", "/" + cmd, true);
    replayCommandRequest.send(null);
}

function close_player()
{
    send_command("close");
}

function reset_player()
{
    send_command("reset");
}

function test_start_player()
{
    var testData = 
        "{" + 
        "\"startPaused\" : true," + 
        "\"startPosition\" : 0," + 
        "\"inputs\": [" + 
        "    {" + 
        "        \"type\" : 9," + 
        "        \"name\" : \"clapper\"" + 
        "    }," + 
        "    {" + 
        "        \"type\" : 7," + 
        "        \"name\" : \"balls\"," + 
        "        \"options\" : {" + 
        "            \"num_balls\" : 10" + 
        "        }" + 
        "    }" + 
        "]" + 
        "}";

    var replayCommandRequest = new XMLHttpRequest();
    replayCommandRequest.open("POST", "/start", true);
    replayCommandRequest.setRequestHeader('Content-Type', 'application/json');    
    replayCommandRequest.send(testData);
}



function send_control_command(cmd)
{
    // cmd must already by URI escaped
    var replayCommandRequest = new XMLHttpRequest();
    replayCommandRequest.open("GET", "/control/" + cmd, true);
    replayCommandRequest.send(null);
}

function play()
{
    send_control_command("play");
}

function pause()
{
    send_control_command("pause");
}

function toggle_play_pause()
{
    send_control_command("toggleplaypause");
}

function step(forwards)
{
    send_control_command("step?forwards=" + (forwards ? "true": "false"));
}

function fast_forward(speed)
{
    send_control_command("playspeed?speed=" + speed);
}

function fast_rewind(speed)
{
    send_control_command("playspeed?speed=" + speed * (-1));
}

function jump_forward(perc)
{
    send_control_command("seek?offset=" + perc * 1000 + "&whence=SEEK_CUR&unit=PERCENTAGE_PLAY_UNIT&pause=true");
}

function jump_backward(perc)
{
    send_control_command("seek?offset=" + perc * 1000 * (-1) + "&whence=SEEK_CUR&unit=PERCENTAGE_PLAY_UNIT&pause=true");
}

function seek_home()
{
    send_control_command("seek?offset=0&whence=SEEK_SET&unit=FRAME_PLAY_UNIT&pause=true");
}

function seek_end()
{
    send_control_command("seek?offset=0&whence=SEEK_END&unit=FRAME_PLAY_UNIT&pause=true");
}


function init_test()
{
    request_player_state();
}



