var statusRequest = null;
var sourceInfoRequest = null;
var recorderControlRequest = null;
var enableRecorderControlRequest = true;

var lastSessionState = null;

// a status update is performed every 1/2 second
var statusInterval = 500;


function set_eta(duration, infaxDuration)
{
    var etaEle = document.getElementById("eta");
    
    if (duration < 0 || infaxDuration < 0)
    {
        update_tag_value("eta", "", "");
        remove_class(etaEle, "eta-zero");
        return;
    }
    
    var eta = infaxDuration - duration;
    if (eta <= 0)
    {
        var etaString = get_duration_string(0);
        update_tag_value_ele(etaEle, etaString, etaString);
        add_class(etaEle, "eta-zero");
    }
    else
    {
        var etaString = get_duration_string(eta);
        update_tag_value_ele(etaEle, etaString, etaString);
        remove_class(etaEle, "eta-zero");
    }
}

function set_browse_line_visibility(visible)
{
    var browseLineE = document.getElementById("browse-line");
    var visibleState = get_tag_state(browseLineE);
    
    if (visibleState == null)
    {
        set_tag_state(browseLineE, true);
    }
    else if (visible && !visibleState)
    {
        browseLineE.setAttribute("style", "");
        set_tag_state(browseLineE, true);
    }
    else if (!visible && visibleState)
    {
        browseLineE.setAttribute("style", "display: none;");
        set_tag_state(browseLineE, false);
    }
}

function recorder_comments_handler()
{}

function set_record_comments(comments)
{
    var recorderCommentsRequest = new XMLHttpRequest();
    recorderCommentsRequest.open("POST", "/recorder/session/setsessioncomments", false);
    recorderCommentsRequest.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');    
    recorderCommentsRequest.onreadystatechange = recorder_comments_handler;
    recorderCommentsRequest.send("comments=" + encodeURIComponent(comments));
}

function set_browse_state(stateOk)
{
    var browseFilenameE = document.getElementById("browse-filename");
    var browseFileSizeE = document.getElementById("browse-filesize");
    if (browseFilenameE == null || browseFileSizeE == null)
    {
        return;
    }
 
    if (stateOk)
    {
        remove_class(browseFilenameE, "browse-overflow");
        remove_class(browseFileSizeE, "browse-overflow");
    }
    else
    {
        add_class(browseFilenameE, "browse-overflow");
        add_class(browseFileSizeE, "browse-overflow");
    }
}
    
function set_button_busy_state(id, busy)
{
    var buttonE = document.getElementById(id);
    if (buttonE == null)
    {
        return;
    }
    
    var state = get_tag_state(buttonE);
    state = (state == NaN) ? -1 : state;
    
    if (busy)
    {
        var nextState = (state == -1) ? statusInterval : (state + statusInterval) % 2001;
        
        if (nextState < 1001)
        {
            if (state < 0 || state >= 1001)
            {
                buttonE.setAttribute("style", "color:#CCCCCC");  // grey colour
            }
        }
        else
        {
            if (state < 1001)
            {
                buttonE.setAttribute("style", "");  // normal colour
            }
        }
        set_tag_state(buttonE, nextState);
    }
    else
    {
        buttonE.setAttribute("style", "");
        set_tag_state(buttonE, -1);
    }
}

function set_record_buttons_state(sessionState)
{
    var recordControlE = document.getElementById("record-control");
    var startRecordButtonE = document.getElementById("start-record-button");
    var stopRecordButtonE = document.getElementById("stop-record-button");
    
    if (recordControlE == null || startRecordButtonE == null || stopRecordButtonE == null)
    {
        return;
    }
    
    var nextTagState;
    switch (sessionState)
    {
        case 1: // READY_SESSION_STATE
            nextTagState = 1;
            break;
        case 2: // RECORDING_SESSION_STATE
            nextTagState = 2;
            break;
        default:
            nextTagState = 0;
            break;
    }
    
    var tagState = get_tag_state(recordControlE);
    
    if (nextTagState == 0) // disable
    {
        if (tagState == null || tagState != nextTagState)
        {
            startRecordButtonE.disabled = "true";
            stopRecordButtonE.disabled = "true";
            set_tag_state(recordControlE, nextTagState);
        }
    }
    else if (nextTagState == 1) // enable start recording
    {
        if (tagState == null || tagState != nextTagState)
        {
            startRecordButtonE.disabled = null;
            stopRecordButtonE.disabled = "true";
            set_tag_state(recordControlE, nextTagState);
        }
    }
    else // nextTagState == 2; enable stop recording
    {
        if (tagState == null || tagState != nextTagState)
        {
            startRecordButtonE.disabled = "true";
            stopRecordButtonE.disabled = null;
            set_tag_state(recordControlE, nextTagState);
        }
    }
}

function set_session_status(status)
{
    if (status == null || 
        (status.sessionStatus.state != 1 && // 1 == READY_SESSION_STATE
            status.sessionStatus.state != 2)) // 2 == RECORDING_SESSION_STATE
    {
        update_tag_value("session-status-message", "", "");
        update_tag_value("d3-vtr-state", "", "");
        update_tag_value("d3-vtr-error-count", "", "");
        update_tag_value("start-vitc", "", "");
        update_tag_value("start-ltc", "", "");
        update_tag_value("current-vitc", "", "");
        update_tag_value("current-ltc", "", "");
        update_tag_value("duration", "", "");
        set_eta(-1, -1);
        update_tag_value("filename", "", "");
        update_tag_value("file-size", "", "");
        update_tag_value("disk-space", "", "");
        set_browse_line_visibility(true);
        update_tag_value("browse-filename", "", "");
        update_tag_value("browse-filesize", "", "");
        set_browse_state(true);
        set_record_buttons_state((status == null) ? 0 : status.sessionStatus.state);
        update_button_disable("abort-button", true);
        set_button_busy_state("start-record-button", false);
        set_button_busy_state("stop-record-button", false);
        set_button_busy_state("abort-button", false);
    }
    else
    {
        update_tag_value("session-status-message", status.sessionStatus.statusMessage, escape(status.sessionStatus.statusMessage));
        update_tag_value("d3-vtr-state", status.sessionStatus.vtrState, escape(status.sessionStatus.vtrState));
        update_tag_value("d3-vtr-error-count", status.sessionStatus.vtrErrorCount, status.sessionStatus.vtrErrorCount);
        update_tag_value("start-vitc", status.sessionStatus.startVITC, escape(status.sessionStatus.startVITC));
        update_tag_value("start-ltc", status.sessionStatus.startLTC, escape(status.sessionStatus.startLTC));
        update_tag_value("current-vitc", status.sessionStatus.currentVITC, escape(status.sessionStatus.currentVITC));
        update_tag_value("current-ltc", status.sessionStatus.currentLTC, escape(status.sessionStatus.currentLTC));
        var durationString = get_duration_string(status.sessionStatus.duration);
        update_tag_value("duration", durationString, durationString);
        set_eta(status.sessionStatus.duration, status.sessionStatus.infaxDuration);
        update_tag_value("filename", status.sessionStatus.filename, escape(status.sessionStatus.filename));
        var fileSizeString = get_size_string(status.sessionStatus.fileSize);
        update_tag_value("file-size", fileSizeString, fileSizeString);
        var diskSpaceString = get_size_string(status.sessionStatus.diskSpace);
        update_tag_value("disk-space", diskSpaceString, diskSpaceString);
        if (status.sessionStatus.browseFilename.length > 0)
        {
            set_browse_line_visibility(true);
            update_tag_value("browse-filename", status.sessionStatus.browseFilename, escape(status.sessionStatus.browseFilename));
            var browseFileSizeString = get_size_string(status.sessionStatus.browseFileSize);
            update_tag_value("browse-filesize", browseFileSizeString, browseFileSizeString);
            set_browse_state(!status.sessionStatus.browseBufferOverflow);
        }
        else
        {
            set_browse_line_visibility(false);
        }
        set_record_buttons_state(status.sessionStatus.state);
        var enableAbort = (status.sessionStatus.state != 0 && // 0 == NOT_STARTED_SESSION_STATE
            status.sessionStatus.state != 6); // 6 == END_SESSION_STATE
        update_button_disable("abort-button", !enableAbort);
        set_button_busy_state("start-record-button", status.sessionStatus.startBusy);
        set_button_busy_state("stop-record-button", status.sessionStatus.stopBusy);
        set_button_busy_state("abort-button", status.sessionStatus.abortBusy);
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
            set_general_status(status, statusInterval);
            
            if (status.sessionStatus.state != 0) // 0 == NOT_STARTED_SESSION_STATE
            {
                // redirect to the review page if the session has stopped recording
                if (lastSessionState == 2 && // 2 == RECORDING_SESSION_STATE
                    (status.sessionStatus.state == 3 || // REVIEWING_SESSION_STATE
                        status.sessionStatus.state == 4 || // 4 == PREPARE_CHUNKING_SESSION_STATE
                        status.sessionStatus.state == 5)) // 5 == CHUNKING_SESSION_STATE
                {
                    // send the comments through to the session
                    var comments;
                    var commentsE = document.getElementById("comments");
                    if (commentsE != null)
                    {
                        comments = commentsE.value;
                        if (comments.length > 0)
                        {
                            set_record_comments(comments);
                        }
                    }
                
                    // move to the review page
                    location.href = "review.html";
                    return;
                }
                lastSessionState = status.sessionStatus.state;
                
                // process session status
                set_session_status(status);
            }
            else
            {
                set_session_status(null);
            }
            
            // set timer for next status request
            statusTimer = setTimeout("request_status()", statusInterval)
        }
    }
    catch (err)
    {
        set_general_status(null, statusInterval);
        set_session_status(null);
        
        // set timer for next status request
        statusTimer = setTimeout("request_status()", statusInterval);
    }
}

function request_status()
{
    statusRequest = new XMLHttpRequest();
    statusRequest.open("GET", "/recorder/status.json?sessionrecord=true", true);
    statusRequest.onreadystatechange = status_handler;
    statusRequest.send(null);
}

function start_record_handler()
{
    try
    {
        if (recorderControlRequest.readyState == 4)
        {
            if (recorderControlRequest.status != 200)
            {
                throw "start record request failed";
            }
            
            enableRecorderControlRequest = true;
        }
    } 
    catch (err)
    {
        enableRecorderControlRequest = true;
        // ignore failure
    }
}

function start_record()
{
    // set a safe keyboard focus
    set_focus("session-link");
    
    if (recorderControlRequest == null || enableRecorderControlRequest)
    {
        enableRecorderControlRequest = false;
        try
        {
            // start recording
            recorderControlRequest = new XMLHttpRequest();
            recorderControlRequest.open("GET", "/recorder/session/start", true);
            recorderControlRequest.onreadystatechange = start_record_handler;
            recorderControlRequest.send(null);
        }
        catch (err)
        {
            enableRecorderControlRequest = true;
        }
    }
}

function stop_record_handler()
{
    try
    {
        if (recorderControlRequest.readyState == 4)
        {
            if (recorderControlRequest.status != 200)
            {
                throw "stop record request failed";
            }
            
            enableRecorderControlRequest = true;
        }
    } 
    catch (err)
    {
        enableRecorderControlRequest = true;
        // ignore failure
    }
}

function stop_record()
{
    // set a safe keyboard focus
    set_focus("session-link");
    
    if (recorderControlRequest == null || enableRecorderControlRequest)
    {
        enableRecorderControlRequest = false;
        try
        {
            // stop recording
            recorderControlRequest = new XMLHttpRequest();
            recorderControlRequest.open("GET", "/recorder/session/stop", true);
            recorderControlRequest.onreadystatechange = stop_record_handler;
            recorderControlRequest.send(null);
        }
        catch (err)
        {
            enableRecorderControlRequest = true;
        }
    }
}

function abort_session_handler()
{
    try
    {
        if (recorderControlRequest.readyState == 4)
        {
            if (recorderControlRequest.status != 200)
            {
                throw "abort session request failed";
            }
            
            enableRecorderControlRequest = true;
        }
    } 
    catch (err)
    {
        enableRecorderControlRequest = true;
        // ignore failure
    }
}

function abort_session()
{
    // set a safe keyboard focus
    set_focus("session-link");
    
    var comments;
    var commentsE = document.getElementById("comments");
    if (commentsE != null)
    {
        comments = commentsE.value;
    }

    if (recorderControlRequest == null || enableRecorderControlRequest)
    {
        enableRecorderControlRequest = false;
        try
        {
            // get user confirmation
            if (!confirm("Please confirm recording session abort"))
            {
                enableRecorderControlRequest = true;
                return
            }
            
            recorderControlRequest = new XMLHttpRequest();
            recorderControlRequest.open("POST", "/recorder/session/abort", true);
            recorderControlRequest.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');    
            recorderControlRequest.onreadystatechange = abort_session_handler;
            recorderControlRequest.send("comments=" + encodeURIComponent(comments));
        }
        catch (err)
        {
            enableRecorderControlRequest = true;
        }
    }
}


function init_record()
{
    // set a safe keyboard focus
    set_focus("session-link");
    
    // get status information from the server
    request_status();
}


