//
// general purpose utility functions
//

function update_button_disable(id, disable)
{
    var disabledVal = (disable) ? "true" : null;
    
    var buttonE = document.getElementById(id);
    if (buttonE != null)
    {
        if (disabledVal != buttonE.getAttribute("disabled"))
        {
            buttonE.disabled = disabledVal;
            return true;
        }
    }
    return false;
}

function update_element_class(id, clss)
{
    var ele = document.getElementById(id);
    if (ele != null)
    {
        if (clss != ele.getAttribute("class"))
        {
            ele.setAttribute("class", clss);
            return true;
        }
    }
    return false;
}

function is_button_disabled(id)
{
    var buttonE = document.getElementById(id);
    if (buttonE == null)
    {
        return true;
    }
    
    return buttonE.getAttribute("disabled") != null;
}

function set_focus(id)
{
    var ele = document.getElementById(id);
    if (ele == null)
    {
        return;
    }
    
    ele.focus();
}

// user defined state property; is typically used to check whether an element 
// needs to be updated 
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

function get_size_string(size)
{
    if (size == null || size < 0)
    {
        return "";
    }
    else if (size < 1000)
    {
        return size + "B";
    }
    else if (size < 1000 * 1000)
    {
        return (size / 1000.0).toFixed(2) + "KB";
    }
    else if (size < 1000 * 1000 * 1000)
    {
        return (size / (1000.0 * 1000.0)).toFixed(2) + "MB";
    }
    else if (size < 1000 * 1000 * 1000 * 1000)
    {
        return (size / (1000.0 * 1000.0 * 1000.0)).toFixed(2) + "GB";
    }
    else
    {
        return (size / (1000.0 * 1000.0 * 1000.0 * 1000.0)).toFixed(2) + "TB";
    }
}

function get_position_string(pos)
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

function get_duration_string(dur)
{
    return get_position_string(dur);
}




//
// General tape transfer status
//

function update_session_link(sessionInProgress)
{
    if (sessionInProgress)
    {
        update_tag_attr("session-link", "style", "color:red");
    }
    else
    {
        update_tag_attr("session-link", "style", null);
    }
}

function status_is_ok(status)
{
    return status != null && status.database && status.tapeDevice;  
}

function set_status(id, isOk)
{
    var statusE = document.getElementById(id);
    
    if (statusE != null)
    {
        if (get_tag_state(statusE) == isOk)
        {
            // no need to change the state 
            return;
        }
        
        if (isOk)
        {
            statusE.setAttribute("style", "");
        }
        else
        {
            statusE.setAttribute("style", "background:red");
        }
        
        set_tag_state(statusE, isOk);
    }
}

function set_general_status(status, statusInterval)
{
    if (status == null)
    {
        // connection is not ok - assume everything not ok
        
        if (document.title != "Ingex Tape Export")
        {
            document.title = "Ingex Tape Export";
        }

        set_status("connection-status", false);
        set_status("database-status", false);
        set_status("tape-device-status", false);
        
        update_session_link(false);
    }
    else
    {
        // connection is ok

        if (document.title != status.recorderName + " - Ingex Tape Export")
        {
            document.title = status.recorderName + " - Ingex Tape Export";
        }

        set_status("connection-status", true);
        set_status("database-status", status.database);
        set_status("tape-device-status", status.tapeDevice);

        update_session_link(status.sessionInProgress);
    }
}


//
// Tape status
//

function get_tape_dev_status_string(status, detail)
{
    var statusStr;
    switch (status)
    {
        case 1: //READY_TAPE_DEV_STATE
            statusStr = "Ready";
            break;
        case 2: //BUSY_TAPE_DEV_STATE
            statusStr = "Busy";
            break;
        case 3: //NO_TAPE_TAPE_DEV_STATE
            statusStr = "No Tape";
            break;
        case 4: // BAD_TAPE_TAPE_DEV_STATE
            statusStr = "Bad Tape";
            break;
        case 0: // UNKNOWN_TAPE_DEV_STATE
        default:
            statusStr = "Unknown";
            break;
    }
    
    if (detail && detail.length > 0)
    {
        statusStr += " - " + detail;
    }
    
    return statusStr;
}


