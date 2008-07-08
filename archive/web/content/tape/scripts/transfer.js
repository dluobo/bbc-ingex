var statusRequest = null;
var abortRequest = null;
var enableAbortRequest = true;
var ltoContentsRequest = null;
var enableLTOContentsRequest = true;

var ltoStatusChangeCount = -1;

// a status update is performed every 1/2 second
var statusInterval = 500;


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

function set_lto_contents(contents)
{
    var ltoContentsE = document.getElementById("lto-contents");
    if (ltoContentsE == null)
    {
        return;
    }

    var newLTOContentsE = document.createElement("table");
    newLTOContentsE.id = "lto-contents";
    
    // headings
    var rowE = document.createElement("tr");
    rowE.setAttribute("style", "color:blue");
    
    var colTransferStatusE = document.createElement("td");
    colTransferStatusE.innerHTML = "Status";
    rowE.appendChild(colTransferStatusE);
    
    var colTransferStartedE = document.createElement("td");
    colTransferStartedE.innerHTML = "Started";
    rowE.appendChild(colTransferStartedE);
    
    var colTransferEndedE = document.createElement("td");
    colTransferEndedE.innerHTML = "Ended";
    rowE.appendChild(colTransferEndedE);
    
    var colFilenameE = document.createElement("td");
    colFilenameE.innerHTML = "Filename";
    rowE.appendChild(colFilenameE);
    
    var colCacheFilenameE = document.createElement("td");
    colCacheFilenameE.innerHTML = "Cache Filename";
    rowE.appendChild(colCacheFilenameE);
    
    var colSizeE = document.createElement("td");
    colSizeE.innerHTML = "Size";
    rowE.appendChild(colSizeE);
    
    var colDurationE = document.createElement("td");
    colDurationE.innerHTML = "Duration";
    rowE.appendChild(colDurationE);
    
    var colSpoolNoE = document.createElement("td");
    colSpoolNoE.innerHTML = "Spool No";
    rowE.appendChild(colSpoolNoE);
    
    var colItemNoE = document.createElement("td");
    colItemNoE.innerHTML = "Item No";
    rowE.appendChild(colItemNoE);
    
    var colProgNoE = document.createElement("td");
    colProgNoE.innerHTML = "Prog No";
    rowE.appendChild(colProgNoE);
    
    newLTOContentsE.appendChild(rowE);
    
    
    // items
    for (i = 0; contents!= null && i < contents.items.length; i++)
    {
        rowE = document.createElement("tr");
        
        colTransferStatusE = document.createElement("td");
        if (contents.items[i].status == 1) // LTO_FILE_TRANSFER_STATUS_NOT_STARTED
        {
            colTransferStatusE.innerHTML = "Not Started";
            colTransferStatusE.setAttribute("class", "ready-status");
        }
        else if (contents.items[i].status == 2) // LTO_FILE_TRANSFER_STATUS_STARTED
        {
            colTransferStatusE.innerHTML = "Started";
            colTransferStatusE.setAttribute("class", "started-status");
        }
        else if (contents.items[i].status == 3) // LTO_FILE_TRANSFER_STATUS_COMPLETED
        {
            colTransferStatusE.innerHTML = "Completed";
            colTransferStatusE.setAttribute("class", "completed-status");
        }
        else // (contents.items[i].status == 4) // LTO_FILE_TRANSFER_STATUS_FAILED
        {
            colTransferStatusE.innerHTML = "Failed";
            colTransferStatusE.setAttribute("class", "failed-status");
        }
        rowE.appendChild(colTransferStatusE);

        colTransferStartedE = document.createElement("td");
        colTransferStartedE.innerHTML = contents.items[i].transferStarted;
        rowE.appendChild(colTransferStartedE);

        colTransferEndedE = document.createElement("td");
        colTransferEndedE.innerHTML = contents.items[i].transferEnded;
        rowE.appendChild(colTransferEndedE);

        colFilenameE = document.createElement("td");
        colFilenameE.innerHTML = contents.items[i].name;
        rowE.appendChild(colFilenameE);

        colCacheFilenameE = document.createElement("td");
        colCacheFilenameE.innerHTML = contents.items[i].cacheName;
        rowE.appendChild(colCacheFilenameE);

        colSizeE = document.createElement("td");
        colSizeE.innerHTML = get_size_string(contents.items[i].size);
        rowE.appendChild(colSizeE);

        colDurationE = document.createElement("td");
        colDurationE.innerHTML = get_duration_string(contents.items[i].duration);
        rowE.appendChild(colDurationE);

        colSpoolNoE = document.createElement("td");
        colSpoolNoE.innerHTML = contents.items[i].srcSpoolNo.replace(/\ /g, "&nbsp;");
        rowE.appendChild(colSpoolNoE);

        colItemNoE = document.createElement("td");
        colItemNoE.innerHTML = contents.items[i].srcItemNo;
        rowE.appendChild(colItemNoE);

        colProgNoE = document.createElement("td");
        colProgNoE.innerHTML = contents.items[i].srcMPProgNo;
        rowE.appendChild(colProgNoE);

        
        newLTOContentsE.appendChild(rowE);
    }
        
    ltoContentsE.parentNode.replaceChild(newLTOContentsE, ltoContentsE);
}

function set_session_status(status)
{
    if (status == null)
    {
        update_button_disable("abort-button", true);
        set_button_busy_state("abort-button", false);
        update_tag_value("session-status", "", 0);
        update_tag_value("tape-dev-status", "", 0);
        update_tag_value("spool-num", "", 0);
        update_tag_value("transfer-method", "Manual", false);
        update_tag_value("total-size", get_size_string(0), 0);
        update_tag_value("num-files", "0", 0);
    }
    else
    {
        update_button_disable("abort-button", !status.enableAbort);
        set_button_busy_state("abort-button", status.abortBusy);
        update_tag_value("session-status", status.sessionStatus, status.sessionStatus);
        var statusString = get_tape_dev_status_string(status.tapeDevStatus, status.tapeDevDetailedStatus);
        update_tag_value("tape-dev-status", statusString, statusString);
        update_tag_value("spool-num", status.spoolNum.replace(/\ /g, "&nbsp;"), status.spoolNum);
        if (status.autoTransferMethod)
        {
            update_tag_value("transfer-method", "Automatic", status.autoTransferMethod);
            if (status.autoSelectionComplete)
            {
                update_tag_value("total-size", get_size_string(status.totalSize), status.totalSize);
                update_tag_value("num-files", status.numFiles, status.numFiles);
            }
            else
            {
                var totalSizeString = get_size_string(status.totalSize) + 
                    " (<" + get_size_string(status.maxTotalSize) + ")";
                var numFilesString = status.numFiles + 
                    " (<" + status.maxFiles + ")";
                update_tag_value("total-size", totalSizeString, totalSizeString);
                update_tag_value("num-files", numFilesString, numFilesString);
            }
        }
        else
        {
            update_tag_value("transfer-method", "Manual", status.autoTransferMethod);
            update_tag_value("total-size", get_size_string(status.totalSize), status.totalSize);
            update_tag_value("num-files", status.numFiles, status.numFiles);
        }
    }
}

function lto_contents_handler()
{
    try
    {
        if (ltoContentsRequest.readyState == 4)
        {
            if (ltoContentsRequest.status != 200)
            {
                throw "failed to get cache contents";
            }
            
            var contents = eval("(" + ltoContentsRequest.responseText + ")");
            set_lto_contents(contents);
            
            ltoStatusChangeCount = contents.ltoStatusChangeCount;
            enableLTOContentsRequest = true;
        }
    } 
    catch (err)
    {
        enableLTOContentsRequest = true;
        // ignore failure
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
            
            if (status.sessionInProgress)
            {
                update_tag_value("session-message", null, 0);
                set_session_status(status.sessionStatus);
                
                if (status.sessionStatus.ltoStatusChangeCount != ltoStatusChangeCount)
                {
                    // lto files status was updated
                    request_lto_contents();
                }
            }
            else
            {
                // set the session message
                var message;
                var messageState;
                var style;
                if (status.hasOwnProperty("lastSession"))
                {
                    if (status.lastSession.completed)
                    {
                        message = "Last session '" + status.lastSession.barcode.replace(/\ /g, "&nbsp;") + "' was completed. ";
                        style = "color:green";
                        messageState = "completed";
                    }
                    else
                    {
                        if (status.lastSession.userAborted)
                        {
                            message = "User aborted last session ";
                        }
                        else
                        {
                            message = "System aborted last session ";
                        }
                        message += "'" + status.lastSession.barcode.replace(/\ /g, "&nbsp;") + "'. ";
                        style = "color:red";
                        messageState = "aborted";
                    }
                }
                else
                {
                    message = "No session is in progress.";
                    style = "color:red";
                    messageState = "no session";
                }
                update_tag_value("session-message", message, messageState);
                update_tag_attr("session-message", "style", style);
                
                set_session_status(null);
                set_lto_contents(null);
            }
            
            // set timer for next status request
            statusTimer = setTimeout("request_status()", statusInterval)
        }
    }
    catch (err)
    {
        set_general_status(null, statusInterval);
        set_session_status(null);
        set_lto_contents(null);
        
        // set timer for next status request
        statusTimer = setTimeout("request_status()", statusInterval);
    }
}

function abort_session_handler()
{
    try
    {
        if (abortRequest.readyState == 4)
        {
            if (abortRequest.status != 200)
            {
                throw "abort session request failed";
            }
            
            enableAbortRequest = true;
        }
    } 
    catch (err)
    {
        enableAbortRequest = true;
        // ignore failure
    }
}


function request_status()
{
    statusRequest = new XMLHttpRequest();
    statusRequest.open("GET", "/tape/status.json?session=true", true);
    statusRequest.onreadystatechange = status_handler;
    statusRequest.send(null);
}

function abort_session()
{
    // set a safe keyboard focus
    set_focus("session-link");
    
    if (abortRequest == null || enableAbortRequest)
    {
        enableRecorderControlRequest = false;
        try
        {
            // get user confirmation
            if (!confirm("Please confirm tape transfer session abort"))
            {
                enableAbortRequest = true;
                return
            }
            
            abortRequest = new XMLHttpRequest();
            abortRequest.open("GET", "/tape/session/abort", true);
            abortRequest.onreadystatechange = abort_session_handler;
            abortRequest.send(null);
        }
        catch (err)
        {
            enableAbortRequest = true;
        }
    }
}

function request_lto_contents()
{
    if (ltoContentsRequest == null || enableLTOContentsRequest)
    {
        enableCacheContentsRequest = false;
        try
        {
            ltoContentsRequest = new XMLHttpRequest();
            ltoContentsRequest.open("GET", "/tape/session/ltocontents.json", true);
            ltoContentsRequest.onreadystatechange = lto_contents_handler;
            ltoContentsRequest.send(null);
        }
        catch (err)
        {
            enableLTOContentsRequest = true;
        }
    }
}



function init_transfer()
{
    // set a safe keyboard focus
    set_focus("session-link");
    
    // get status information from the server
    request_status();
}


