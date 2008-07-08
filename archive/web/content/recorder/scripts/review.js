var statusRequest = null;
var recorderControlRequest = null;
var enableRecorderControlRequest = true;
var sessionCommentsRequest = null;
var enableSessionCommentsRequest = true;
var sessionCommentsCount = 0;
var itemMarkRequest = null;
var enableItemMarkRequest = true;

var clearItem = new Object;
clearItem.id = -1;
clearItem.index = -1;

// a status update is performed every 1/2 second
var statusInterval = 500;


function update_progressbar_marks_handler()
{
    try
    {
        if (itemMarkRequest.readyState == 4)
        {
            if (itemMarkRequest.status != 200)
            {
                throw "item clip info request status not ok";
            }
            
            var itemClipInfo = eval("(" + itemMarkRequest.responseText + ")");
            
            if (pgItemClipChangeCount != itemClipInfo.itemClipChangeCount ||
                pgMarksDuration != itemMarkRequest.duration)
            {
                init_progressbar_marks(itemClipInfo, itemMarkRequest.duration);
            }

            enableItemMarkRequest = true;
        }
    } 
    catch (err)
    {
        enableItemMarkRequest = true;
    }
}

function update_progressbar_marks(duration)
{
    if (enableItemMarkRequest)
    {
        enableItemMarkRequest = false;
        try
        {
            itemMarkRequest = new XMLHttpRequest();
            itemMarkRequest.duration = duration;
            itemMarkRequest.open("GET", "/recorder/session/getitemclipinfo.json", true);
            itemMarkRequest.onreadystatechange = update_progressbar_marks_handler;
            itemMarkRequest.send(null);
        }
        catch (err)
        {
            enableItemMarkRequest = true;
        }
    }
}

function reset_progressbar_marks()
{
    if (enableItemMarkRequest)
    {
        enableItemMarkRequest = false;
        try
        {
            init_progressbar_marks(null, -1);
            enableItemMarkRequest = true;
        }
        catch (err)
        {
            enableItemMarkRequest = true;
        }
    }
}

function set_last_session_result(result, sourceSpoolNo, failureReason)
{
    var resultText;    
    switch (result)
    {
        case 1: // 1 == COMPLETED_SESSION_RESULT
            resultText = "<span class='last-session-completed-result'>Successfully completed ingest of '" + 
                sourceSpoolNo.replace(/\ /g, "&nbsp;") + "'</span>";  
            if (update_tag_value("last-session-result", resultText, escape(resultText)))
            {
                update_tag_attr("last-session-result", "style", "");
            }
            break;
            
        case 2: // 2 == FAILED_SESSION_RESULT
            resultText = "<span class='last-session-failed-result'>Failed to complete ingest of '" + 
                sourceSpoolNo.replace(/\ /g, "&nbsp;") + "': " + failureReason + "</span>";  
            if (update_tag_value("last-session-result", resultText, escape(resultText)))
            {
                update_tag_attr("last-session-result", "style", "");
            }
            break;
            
        case 0: // 0 == UNKNOWN_SESSION_RESULT
        default:
            update_tag_attr("last-session-result", "style", "display: none;");
            break;
    }
}

function set_clear_item_state(isAtStartOfItem, itemId, itemIndex)
{
    if (isAtStartOfItem && itemIndex > 0)
    {
        clearItem.id = itemId;
        clearItem.index = itemIndex;
        update_button_disable("clear-mark-button", false);
    }
    else
    {
        clearItem.id = -1;
        clearItem.index = -1;
        update_button_disable("clear-mark-button", true);
    }
}

function set_play_info(itemIndex, itemCount, itemPosition, filePosition)
{
    if (itemIndex >= 0)
    {
        var itemStr = parseInt(itemIndex + 1) + " of " + itemCount;
        update_tag_value("play-item-index", itemStr, itemStr);
    }
    else
    {
        update_tag_value("play-item-index", "", "");
    }
    if (itemPosition >= 0)
    {
        update_tag_value("play-item-position", get_position_string(itemPosition), get_position_string(itemPosition));
    }
    else
    {
        update_tag_value("play-item-position", "", "");
    }
    if (filePosition >= 0)
    {
        update_tag_value("play-file-position", get_position_string(filePosition), get_position_string(filePosition));
    }
    else
    {
        update_tag_value("play-file-position", "", "");
    }
}

function set_session_comments_text(comments, count)
{
    if (sessionCommentsCount != count)
    {
        var commentsE = document.getElementById("comments");
        commentsE.value = comments;
        sessionCommentsCount = count;
    }
}

function set_pse_result(pseResult)
{
    var pseResultE = document.getElementById("pse-result");
    
    if (pseResult == null)
    {
        pseResultE.innerHTML = "";
        pseResultE.setAttribute("style", "");
        set_tag_state(pseResultE, -1);
        return;
    }
    
    switch (pseResult)
    {
        case 1: // PSE_RESULT_PASSED
            if (get_tag_state(pseResultE) != pseResult)
            {
                pseResultE.innerHTML = "Passed";
                pseResultE.setAttribute("style", "color: green");
                set_tag_state(pseResultE, pseResult);
            }
            break;
        case 2: // PSE_RESULT_FAILED
            if (get_tag_state(pseResultE) != pseResult)
            {
                pseResultE.innerHTML = "FAILED";
                pseResultE.setAttribute("style", "color: red");
                set_tag_state(pseResultE, pseResult);
            }
        case 0:
        default:
            if (get_tag_state(pseResultE) != pseResult)
            {
                pseResultE.innerHTML = "?Unknown";
                pseResultE.setAttribute("style", "");
                set_tag_state(pseResultE, pseResult);
            }
    }
}

function update_conf_replay_state(hide)
{
    var confReplayButtonsE = document.getElementById("conf-replay-buttons");
    if (confReplayButtonsE == null)
    {
        return;
    }
    
    var stateA = get_tag_state(confReplayButtonsE);
    if (stateA != null && stateA == hide)
    {
        // no update required
        return;
    }

    if (hide)
    {
        confReplayButtonsE.setAttribute("style", "visibility: hidden");
        set_tag_state(confReplayButtonsE, hide);
    }
    else
    {
        // set focus on the play buttons (or any button on the page) to get 
        // keyboard focus working for the shuttle control
        // also safer not to allow focus on Abort/Complete
        set_focus("play-cmd");
        
        confReplayButtonsE.setAttribute("style", null);
        set_tag_state(confReplayButtonsE, hide);
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

function session_comments_handler()
{
    try
    {
        if (sessionCommentsRequest.readyState == 4)
        {
            if (sessionCommentsRequest.status != 200)
            {
                throw "source info request status not ok";
            }
            
            var commentsInfo = eval("(" + sessionCommentsRequest.responseText + ")");

            set_session_comments_text(commentsInfo.comments, commentsInfo.count);

            enableSessionCommentsRequest = true;
        }
    } 
    catch (err)
    {
        // ignore failure
        enableSessionCommentsRequest = true;
    }
}

function request_session_comments()
{
    if (sessionCommentsRequest == null || enableSessionCommentsRequest)
    {
        enableSessionCommentsRequest = false;
        try
        {
            sessionCommentsRequest = new XMLHttpRequest();
            sessionCommentsRequest.open("GET", "/recorder/session/getsessioncomments.json", true);
            sessionCommentsRequest.onreadystatechange = session_comments_handler;
            sessionCommentsRequest.send(null);
        }
        catch (err)
        {
            enableSessionCommentsRequest = true;
        }
    }
}

function set_session_status(status)
{
    var sourceInfoE = document.getElementById("tape-info");
    
    if (status == null || 
        (status.sessionStatus.state != 3 && // 3 == REVIEWING_SESSION_STATE
            status.sessionStatus.state != 4 && // 4 == PREPARE_CHUNKING_SESSION_STATE
            status.sessionStatus.state != 5)) // 5 == CHUNKING_SESSION_STATE
    {
        update_tag_value("d3-spool-no", "", "");
        update_tag_value("digibeta-spool-no", "", "");
        update_tag_value("d3-vtr-error-count", "", "");
        set_pse_result(null);
        update_tag_value("duration", "", "");
        update_tag_value("infax-duration", "", "");
        update_button_disable("abort-button", true);
        update_button_disable("complete-button", true);
        update_button_disable("chunk-button", true);
        set_button_busy_state("abort-button", false);
        set_button_busy_state("complete-button", false);
        set_button_busy_state("chunk-button", false);
        set_play_info(-1, -1, -1, -1);
        update_button_disable("mark-start-button", true);
        set_clear_item_state(false, -1, -1);
        update_button_disable("prev-mark-button", true);
        update_button_disable("next-mark-button", true);
        set_session_comments_text("", 0);
        set_progress_bar_state(0, -1, -1);
        if (status != null && status.sessionStatus.state == 0)
        {
            set_last_session_result(status.sessionStatus.lastSessionResult, status.sessionStatus.lastSessionSourceSpoolNo, status.sessionStatus.lastSessionFailureReason);
        }
        else
        {
            set_last_session_result(0, "", "");
        }
        update_progressbar_pointer(-1, -1);
        reset_progressbar_marks();
    }
    else
    {
        update_tag_value("d3-spool-no", status.sessionStatus.d3SpoolNo.replace(/\ /g, "&nbsp;"), status.sessionStatus.d3SpoolNo);
        update_tag_value("digibeta-spool-no", status.sessionStatus.digibetaSpoolNo.replace(/\ /g, "&nbsp;"), status.sessionStatus.digibetaSpoolNo);
        update_tag_value("d3-vtr-error-count", status.sessionStatus.vtrErrorCount, escape(status.sessionStatus.vtrErrorCount));
        set_pse_result(status.sessionStatus.pseResult);
        var durationString = get_duration_string(status.sessionStatus.duration);
        update_tag_value("duration", durationString, durationString);
        var infaxDurationString = get_duration_string(status.sessionStatus.infaxDuration);
        update_tag_value("infax-duration", infaxDurationString, infaxDurationString);
        var enableAbort = (status.sessionStatus.state != 0 && // 0 == NOT_STARTED_SESSION_STATE
            status.sessionStatus.state != 6); // 6 == END_SESSION_STATE
        update_button_disable("abort-button", !enableAbort);
        var enableComplete = (status.sessionStatus.state == 3); // 3 == REVIEWING_SESSION_STATE
        update_button_disable("complete-button", !enableComplete);
        update_button_disable("chunk-button", !status.sessionStatus.readyToChunk);
        set_button_busy_state("abort-button", status.sessionStatus.abortBusy);
        set_button_busy_state("complete-button", status.sessionStatus.completeBusy);
        set_button_busy_state("chunk-button", status.sessionStatus.chunkBusy);
        set_play_info(status.sessionStatus.playingItemIndex, status.sessionStatus.itemCount, status.sessionStatus.playingItemPosition, status.sessionStatus.playingFilePosition);
        var enableMarkStart = status.sessionStatus.state == 4 && // 4 == PREPARE_CHUNKING_SESSION_STATE
            status.sessionStatus.playingItemPosition != 0 && // is not at the start of the item
            !status.sessionStatus.readyToChunk; // items left to mark
        update_button_disable("mark-start-button", !enableMarkStart);
        if (status.sessionStatus.state == 4) // 4 == PREPARE_CHUNKING_SESSION_STATE
        {
            set_clear_item_state(status.sessionStatus.playingItemPosition == 0, status.sessionStatus.playingItemId, status.sessionStatus.playingItemIndex);
        }
        else
        {
            set_clear_item_state(false, -1, -1);
        }
        update_button_disable("prev-mark-button", status.sessionStatus.itemCount <= 1);
        update_button_disable("next-mark-button", status.sessionStatus.itemCount <= 1);
        if (status.sessionStatus.sessionCommentsCount != sessionCommentsCount)
        {
            // get updated session comments
            request_session_comments();
        }
        set_progress_bar_state(status.sessionStatus.state, status.sessionStatus.chunkingPosition, status.sessionStatus.chunkingDuration);
        if (status.sessionStatus.state == 0)
        {
            set_last_session_result(status.sessionStatus.lastSessionResult, status.sessionStatus.lastSessionSourceSpoolNo, status.sessionStatus.lastSessionFailureReason);
        }
        else
        {
            set_last_session_result(0, "", "");
        }
        update_progressbar_pointer(status.sessionStatus.playingFilePosition, status.sessionStatus.playingFileDuration);
        var marksDuration;
        if (status.sessionStatus.state == 5) // 5 == CHUNKING_SESSION_STATE
        {
            marksDuration = status.sessionStatus.duration;
        }
        else
        {
            marksDuration = status.sessionStatus.playingFileDuration;
        }
        if (pgItemClipChangeCount != status.sessionStatus.itemClipChangeCount ||
            pgMarksDuration != marksDuration)
        {
            update_progressbar_marks(marksDuration);
        }
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
            
            set_session_status(status);
            
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
    statusRequest.open("GET", "/recorder/status.json?sessionreview=true", true);
    statusRequest.onreadystatechange = status_handler;
    statusRequest.send(null);
}

function complete_session_handler()
{
    try
    {
        if (recorderControlRequest.readyState == 4)
        {
            if (recorderControlRequest.status != 200)
            {
                throw "complete session request failed";
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

function complete_session()
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
            recorderControlRequest = new XMLHttpRequest();
            recorderControlRequest.open("POST", "/recorder/session/complete", true);
            recorderControlRequest.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');    
            recorderControlRequest.onreadystatechange = complete_session_handler;
            recorderControlRequest.send("comments=" + encodeURIComponent(comments));
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

function play_prev_item()
{
    // set a safe keyboard focus
    set_focus("play-cmd");
    
    var itemPlayRequest = new XMLHttpRequest();
    itemMarkRequest.open("GET", "/recorder/session/playprevitem", true);
    itemMarkRequest.send(null);
}

function mark_item_start_handler()
{
    try
    {
        if (itemMarkRequest.readyState == 4)
        {
            if (itemMarkRequest.status != 200)
            {
                throw "mark item start request failed";
            }
            
            var result = eval("(" + itemMarkRequest.responseText + ")");

            // TODO: update stuff instead of waiting for the next status poll
            
            enableItemMarkRequest = true;
        }
    } 
    catch (err)
    {
        enableItemMarkRequest = true;
        // ignore failure
    }
}

function mark_item_start()
{
    // set a safe keyboard focus
    set_focus("play-cmd");
    
    if (itemMarkRequest == null || enableItemMarkRequest)
    {
        enableItemMarkRequest = false;
        try
        {
            itemMarkRequest = new XMLHttpRequest();
            itemMarkRequest.open("GET", "/recorder/session/markitemstart.json", true);
            itemMarkRequest.onreadystatechange = mark_item_start_handler;
            itemMarkRequest.send(null);
        }
        catch (err)
        {
            enableItemMarkRequest = true;
        }
    }
}

function clear_item_handler()
{
    try
    {
        if (itemMarkRequest.readyState == 4)
        {
            if (itemMarkRequest.status != 200)
            {
                throw "clear item request failed";
            }
            
            var result = eval("(" + itemMarkRequest.responseText + ")");
            
            // TODO: update stuff instead of waiting for the next status poll
            
            enableItemMarkRequest = true;
        }
    } 
    catch (err)
    {
        enableItemMarkRequest = true;
        // ignore failure
    }
}

function clear_item()
{
    // set a safe keyboard focus
    set_focus("play-cmd");
    
    if (itemMarkRequest == null || enableItemMarkRequest)
    {
        enableItemMarkRequest = false;
        try
        {
            if (clearItem.id < 0)
            {
                enableItemMarkRequest = true;
                return;
            }
        
            itemMarkRequest = new XMLHttpRequest();
            itemMarkRequest.open("GET", "/recorder/session/clearitem.json?id=" + clearItem.id + "&index=" + clearItem.index, true);
            itemMarkRequest.onreadystatechange = clear_item_handler;
            itemMarkRequest.send(null);
        }
        catch (err)
        {
            enableItemMarkRequest = true;
        }
    }
}

function play_next_item()
{
    // set a safe keyboard focus
    set_focus("play-cmd");
    
    var itemPlayRequest = new XMLHttpRequest();
    itemPlayRequest.open("GET", "/recorder/session/playnextitem", true);
    itemPlayRequest.send(null);
}

function seek_to_eop()
{
    // set a safe keyboard focus
    set_focus("play-cmd");
    
    var itemPlayRequest = new XMLHttpRequest();
    itemPlayRequest.open("GET", "/recorder/session/seektoeop", true);
    itemPlayRequest.send(null);
}


function chunk_file_handler()
{
    try
    {
        if (recorderControlRequest.readyState == 4)
        {
            if (recorderControlRequest.status != 200)
            {
                throw "chunk file request failed";
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

function chunk_file()
{
    // set a safe keyboard focus
    set_focus("session-link");
    
    if (recorderControlRequest == null || enableRecorderControlRequest)
    {
        enableRecorderControlRequest = false;
        try
        {
            // get user confirmation
            if (!confirm("Please confirm to start chunking"))
            {
                enableRecorderControlRequest = true;
                return;
            }
            
            recorderControlRequest = new XMLHttpRequest();
            recorderControlRequest.open("GET", "/recorder/session/chunkfile", true);
            recorderControlRequest.onreadystatechange = chunk_file_handler;
            recorderControlRequest.send(null);
        }
        catch (err)
        {
            enableRecorderControlRequest = true;
        }
    }
}

function init_review()
{
    // set a safe keyboard focus
    set_focus("session-link");
    
    // reset session comments
    set_session_comments_text("", 0);

    // override replay command to set and remove marks
    set_user_mark = mark_item_start;
    remove_user_mark = clear_item;
    
    // get status information from the server
    request_status();
}


