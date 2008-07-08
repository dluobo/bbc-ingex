
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


