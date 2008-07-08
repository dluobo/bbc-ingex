var statusRequest = null;
var vtrControlRequest = null;
var enableVTRControlRequest = true;


// a status update is performed every 1/2 second
var statusInterval = 500;


function get_device_type_string(deviceType, deviceTypeCode)
{
    return deviceType + " (0x" + convert_to_hex(deviceTypeCode, 2) + ")";
}

function get_state_string(state)
{
    switch (state)
    {
        case 0:
            return "Not Connected";
        case 1:
            return "Remote Lockout";
        case 2:
            return "No Tape";
        case 3:
            return "Stopped";
        case 4:
            return "Paused";
        case 5:
            return "Playing";
        case 6:
            return "Fast Forwarding";
        case 7:
            return "Fast Rewinding";
        case 8:
            return "Ejecting";
        case 9:
            return "Recording";
        case 10:
            return "Seeking";
        default:
            return "Not recognized";
    }
}

function get_timecode_string(tc)
{
    return "" + 
        ((tc.hour >= 10) ? tc.hour : "0" + tc.hour) + ":" + 
        ((tc.min >= 10) ? tc.min : "0" + tc.min) + ":" + 
        ((tc.sec >= 10) ? tc.sec : "0" + tc.sec) + ":" + 
        ((tc.frame >= 10) ? tc.frame : "0" + tc.frame); 
}

function get_error_string(errorCode)
{
    if ((errorCode & 0x77) == 0x00)
    {
        return "None";
    }
    
    var result = "";
    switch (errorCode & 0x07)
    {
        case 0x00: // video ok
            break;
        case 0x01:
            result = "Video Almost Good";
            break;
        case 0x02:
            result = "Video Cannot Be Determined";
            break;
        case 0x03:
            result = "Video Unclear";
            break;
        case 0x04:
            result = "Video Not Good";
            break;
    }
    
    if (result.length > 0 && errorCode & 0x70 != 0x00)
    {
        result += ", ";
    }
    switch (errorCode & 0x70)
    {
        case 0x00: // audio ok
            break;
        case 0x10:
            result += "Audio Almost Good";
            break;
        case 0x20:
            result += "Audio Cannot Be Determined";
            break;
        case 0x30:
            result += "Audio Unclear";
            break;
        case 0x40:
            result += "Audio Not Good";
            break;
    }
    
    return result;
}

function set_button_state(ele, active)
{
    if (active)
    {
        if (get_tag_state(ele) != active)
        {
            ele.setAttribute("class", "active-button");
        }
    }
    else
    {
        if (get_tag_state(ele) != active)
        {
            ele.setAttribute("class", null);
        }
    }
    set_tag_state(ele, active);
}

function set_status(status)
{
    var frButtonE = document.getElementById("fr-button");
    var playButtonE = document.getElementById("play-button");
    var stopButtonE = document.getElementById("stop-button");
    var standbyButtonE = document.getElementById("standby-button");
    var recordButtonE = document.getElementById("record-button");
    var ffButtonE = document.getElementById("ff-button");
    var ejectButtonE = document.getElementById("eject-button");
    
    if (status == null)
    {
        update_tag_value("status", get_state_string(0), get_state_string(0));
        update_tag_value("device-type", get_device_type_string("Unknown", 0), 0);
        update_tag_value("vitc", "00:00:00:00", "00:00:00:00");
        update_tag_value("ltc", "00:00:00:00", "00:00:00:00");
        update_tag_value("playback-error", get_error_string(0), 0);
        update_tag_value("playback-error-ltc", "00:00:00:00", "00:00:00:00");
        
        set_button_state(frButtonE, false);
        set_button_state(playButtonE, false);
        set_button_state(stopButtonE, false);
        set_button_state(standbyButtonE, false);
        set_button_state(recordButtonE, false);
        set_button_state(ffButtonE, false);
        set_button_state(ejectButtonE, false);
        
        update_button_disable("record-button", true);
        return;
    }

    update_tag_value("status", get_state_string(status.state), get_state_string(status.state));
    update_tag_value("device-type", get_device_type_string(status.deviceType, status.deviceTypeCode), status.deviceTypeCode);
    update_tag_value("vitc", get_timecode_string(status.vitc), get_timecode_string(status.vitc));
    update_tag_value("ltc", get_timecode_string(status.ltc), get_timecode_string(status.ltc));
    update_tag_value("playback-error", get_error_string(status.errorCode), get_error_string(status.errorCode));
    update_tag_value("playback-error-ltc", get_timecode_string(status.errorLTC), get_timecode_string(status.errorLTC));
    
    update_button_disable("record-button", !status.recordEnabled);
    
    switch (status.state)
    {
        case 3: // stopped
            set_button_state(frButtonE, false);
            set_button_state(playButtonE, false);
            set_button_state(stopButtonE, true);
            set_button_state(standbyButtonE, false);
            set_button_state(recordButtonE, false);
            set_button_state(ffButtonE, false);
            set_button_state(ejectButtonE, false);
            break;
        case 4: // paused
            set_button_state(frButtonE, false);
            set_button_state(playButtonE, false);
            set_button_state(stopButtonE, true);
            set_button_state(standbyButtonE, true);
            set_button_state(recordButtonE, false);
            set_button_state(ffButtonE, false);
            set_button_state(ejectButtonE, false);
            break;
        case 5: // play
            set_button_state(frButtonE, false);
            set_button_state(playButtonE, true);
            set_button_state(stopButtonE, false);
            set_button_state(standbyButtonE, true);
            set_button_state(recordButtonE, false);
            set_button_state(ffButtonE, false);
            set_button_state(ejectButtonE, false);
            break;
        case 6: // fast forward
            set_button_state(frButtonE, false);
            set_button_state(playButtonE, false);
            set_button_state(stopButtonE, false);
            set_button_state(standbyButtonE, true);
            set_button_state(recordButtonE, false);
            set_button_state(ffButtonE, true);
            set_button_state(ejectButtonE, false);
            break;
        case 7: // fast rewind
            set_button_state(frButtonE, true);
            set_button_state(playButtonE, false);
            set_button_state(stopButtonE, false);
            set_button_state(standbyButtonE, true);
            set_button_state(recordButtonE, false);
            set_button_state(ffButtonE, false);
            set_button_state(ejectButtonE, false);
            break;
        case 8: // eject
            set_button_state(frButtonE, false);
            set_button_state(playButtonE, false);
            set_button_state(stopButtonE, false);
            set_button_state(standbyButtonE, false);
            set_button_state(recordButtonE, false);
            set_button_state(ffButtonE, false);
            set_button_state(ejectButtonE, true);
            break;
        case 9: // recording
            set_button_state(frButtonE, false);
            set_button_state(playButtonE, false);
            set_button_state(stopButtonE, false);
            set_button_state(standbyButtonE, true);
            set_button_state(recordButtonE, true);
            set_button_state(ffButtonE, false);
            set_button_state(ejectButtonE, false);
            break;
        default:
            set_button_state(frButtonE, false);
            set_button_state(playButtonE, false);
            set_button_state(stopButtonE, false);
            set_button_state(standbyButtonE, false);
            set_button_state(recordButtonE, false);
            set_button_state(ffButtonE, false);
            set_button_state(ejectButtonE, false);
            break;
    }
}

function status_handler()
{
    try
    {
        if (statusRequest.readyState == 4)
        {
            if (statusRequest.status != 200)
            {
                throw "request status not ok";
            }
            
            var status = eval("(" + statusRequest.responseText + ")");
            
            set_status(status);
            
            // set timer for next status request
            statusTimer = setTimeout("request_status()", statusInterval)
        }
    }
    catch (err)
    {
        set_status(null);
        
        // set timer for next status request
        statusTimer = setTimeout("request_status()", statusInterval);
    }
}

function request_status()
{
    statusRequest = new XMLHttpRequest();
    statusRequest.open("GET", "/vtr/status.json", true);
    statusRequest.onreadystatechange = status_handler;
    statusRequest.send(null);
}

function vtr_control_handler()
{
    try
    {
        if (vtrControlCommandRequest.readyState == 4)
        {
            if (vtrControlCommandRequest.status != 200)
            {
                throw "request status not ok";
            }
            
            enableVTRControlRequest = true;
        }
    }
    catch (err)
    {
        enableVTRControlRequest = true;
    }
}

function vtr_control_command(cmd)
{
    if (vtrControlRequest == null || enableVTRControlRequest)
    {
        enableVTRControlRequest = false;
        try
        {
            vtrControlCommandRequest = new XMLHttpRequest();
            vtrControlCommandRequest.open("GET", "/vtr/control/" + cmd, true);
            vtrControlCommandRequest.onreadystatechange = vtr_control_handler;
            vtrControlCommandRequest.send(null);
        }
        catch (err)
        {
            enableVTRControlRequest = true;
        }
    }
    
}

function toggle_standby()
{
    var standbyButtonE = document.getElementById("standby-button");
    if (standbyButtonE == null)
    {
        return;
    }

    if (get_tag_state(standbyButtonE))
    {
        vtr_control_command('standbyoff');
    }
    else
    {
        vtr_control_command('standbyon');
    }
}

function init_index()
{
    // get status information from the server
    request_status();
}


