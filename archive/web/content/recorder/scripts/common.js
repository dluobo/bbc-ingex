//
// general purpose utility functions
//

function convert_to_hex(value, minWidth)
{
    var hexValue = value.toString(16);
    while (hexValue.length < minWidth)
    {
        hexValue = '0' + hexValue;
    }
    return hexValue.toUpperCase();
}

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

function get_size_string(size)
{
    if (size < 0)
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

function have_class(ele, klass)
{
    var classAttr = ele.getAttribute("class");
    if (classAttr == null)
    {
        return false;
    }
    
    var classes = classAttr.split(" ");
    var i;
    for (i = 0; i < classes.length; i++)
    {
        if (classes[i] == klass)
        {
            return true;
        }
    }
    
    return false;
}

function add_class(ele, klass)
{
    var currentClass = ele.getAttribute("class");
    if (currentClass == null)
    {
        ele.setAttribute("class", klass);
        return;
    }
    
    var classes = currentClass.split(" ");
    var i;
    for (i = 0; i < classes.length; i++)
    {
        if (classes[i] == klass)
        {
            return;
        }
    }
    classes.push(klass);
    
    ele.setAttribute("class", classes.join(" "));
}

function remove_class(ele, klass)
{
    var currentClass = ele.getAttribute("class");
    if (currentClass == null)
    {
        return;
    }
    
    var classes = currentClass.split(" ");
    var found = false;
    var i = 0;
    while (i < classes.length)
    {
        if (classes[i] == klass)
        {
            delete classes[i];
            found = true;
        }
        else
        {
            i++;
        }
    }
    if (!found)
    {
        return;
    }
    
    ele.setAttribute("class", classes.join(" "));
}



//
// General recorder status
//


function update_active_link(sessionState)
{
    var recordLinkE = document.getElementById("record-link");
    var reviewLinkE = document.getElementById("review-link");
    
    switch (sessionState)
    {
        case 1: // 1 == READY_SESSION_STATE
        case 2: // 2 == RECORDING_SESSION_STATE
            add_class(recordLinkE, "active-page-link");
            remove_class(reviewLinkE, "active-page-link");
            break;
        case 3: // 3 == REVIEWING_SESSION_STATE
        case 4: // 4 == PREPARE_CHUNKING_SESSION_STATE
        case 5: // 5 == CHUNKING_SESSION_STATE
            add_class(reviewLinkE, "active-page-link");
            remove_class(recordLinkE, "active-page-link");
            break;
        default:
            remove_class(reviewLinkE, "active-page-link");
            remove_class(recordLinkE, "active-page-link");
            break;
    }
}

function status_is_ok(status)
{
    return status != null && 
        status.database && 
        status.sdiCard && 
        status.video && 
        status.audio &&
        status.vtr;  
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
        
        if (document.title != "Ingex Recorder")
        {
            document.title = "Ingex Recorder";
        }

        set_status("connection-status", false);
        set_status("database-status", false);
        set_status("sdi-card-status", false);
        set_status("video-status", false);
        set_status("audio-status", false);
        set_status("vtr-status", false);
        
        update_active_link(0);
    }
    else
    {
        // connection is ok

        if (document.title != status.recorderName + " - Ingex Recorder")
        {
            document.title = status.recorderName + " - Ingex Recorder";
        }

        set_status("connection-status", true);
        set_status("database-status", status.database);
        set_status("sdi-card-status", status.sdiCard);
        set_status("video-status", status.video);
        set_status("audio-status", status.audio);
        set_status("vtr-status", status.vtr);

        update_active_link(status.sessionState);
    }
}


//
// Replay buttons
//

function flat_button_down(ele)
{
    ele.parentNode.setAttribute("style", "border-color: OrangeRed");
}

function flat_button_up(ele)
{
    ele.parentNode.setAttribute("style", "");
}


function item_control_down(ele)
{
    ele.parentNode.setAttribute("style", "background-color: OrangeRed");
}

function item_control_up(ele)
{
    ele.parentNode.setAttribute("style", "");
}

