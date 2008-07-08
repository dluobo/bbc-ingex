var cacheContentsRequest = null;
var enableCacheContentsRequest = true;
var cacheStatusChangeCount = -1;
var deleteItemsRequest = null;
var enableDeleteItemsRequest = true;
var playFileRequest = null;
var enablePlayFileRequest = true;

// a status update is performed every 1/2 second
var statusInterval = 500;


function update_replay_state(active, filename)
{
    var confReplayButtonsE = document.getElementById("conf-replay-buttons");
    if (confReplayButtonsE == null)
    {
        return;
    }
    
    // hide or show
    var stateA = get_tag_state(confReplayButtonsE);
    if (stateA == null || stateA != active)
    {
        if (active)
        {
            confReplayButtonsE.setAttribute("style", "display: block");
            set_tag_state(confReplayButtonsE, active);

            set_focus("play-cmd");
            window.scroll(0, 0);
        }
        else
        {
            confReplayButtonsE.setAttribute("style", "display: none");
            set_tag_state(confReplayButtonsE, active);
        }
    }
    
    // replay filename
    var wasUpdated = update_tag_value("replay-filename", filename, filename);
    if (wasUpdated)
    {   
        set_focus("play-cmd");
        window.scroll(0, 0);
    }
}

function set_cache_contents(contents)
{
    var cacheContentsE = document.getElementById("cache-contents");

    var newCacheContentsE = document.createElement("table");
    newCacheContentsE.id = "cache-contents";
    
    // headings
    var rowE = document.createElement("tr");
    rowE.setAttribute("style", "color:blue");
    
    var colSpoolNoE = document.createElement("td");
    colSpoolNoE.innerHTML = "Spool No";
    rowE.appendChild(colSpoolNoE);
    
    var colItemNoE = document.createElement("td");
    colItemNoE.innerHTML = "Item No";
    rowE.appendChild(colItemNoE);
    
    var colProgNoE = document.createElement("td");
    colProgNoE.innerHTML = "Prog No";
    rowE.appendChild(colProgNoE);
    
    var colTransferDateE = document.createElement("td");
    colTransferDateE.innerHTML = "Ingest Date";
    rowE.appendChild(colTransferDateE);
    
    var colTransferStatusE = document.createElement("td");
    colTransferStatusE.innerHTML = "Ingest Status";
    rowE.appendChild(colTransferStatusE);
    
    var colFilenameE = document.createElement("td");
    colFilenameE.innerHTML = "Filename";
    rowE.appendChild(colFilenameE);
    
    var colSizeE = document.createElement("td");
    colSizeE.innerHTML = "Size";
    rowE.appendChild(colSizeE);
    
    var colDurationE = document.createElement("td");
    colDurationE.innerHTML = "Duration";
    rowE.appendChild(colDurationE);
    
    var colPSEReportE = document.createElement("td");
    colPSEReportE.innerHTML = "PSE Report";
    rowE.appendChild(colPSEReportE);
    
    var colPlayE = document.createElement("td");
    colPlayE.innerHTML = "Play";
    rowE.appendChild(colPlayE);
    
    newCacheContentsE.appendChild(rowE);
    
    
    // items
    for (i = 0; i < contents.items.length; i++)
    {
        rowE = document.createElement("tr");
        
        colSpoolNoE = document.createElement("td");
        colSpoolNoE.innerHTML = contents.items[i].srcSpoolNo.replace(/\ /g, "&nbsp;");
        rowE.appendChild(colSpoolNoE);

        colItemNoE = document.createElement("td");
        colItemNoE.innerHTML = contents.items[i].srcItemNo;
        rowE.appendChild(colItemNoE);

        colProgNoE = document.createElement("td");
        colProgNoE.innerHTML = contents.items[i].srcMPProgNo;
        rowE.appendChild(colProgNoE);

        colTransferDateE = document.createElement("td");
        colTransferDateE.innerHTML = contents.items[i].sessionCreation;
        rowE.appendChild(colTransferDateE);

        colTransferStatusE = document.createElement("td");
        colTransferStatusE.innerHTML = contents.items[i].sessionStatusString;
        if (contents.items[i].sessionStatus == 1) // STARTED
        {
            colTransferStatusE.setAttribute("class", "started-status");
        }
        else if (contents.items[i].sessionStatus == 2) // COMPLETED
        {
            colTransferStatusE.setAttribute("class", "completed-status");
        }
        else // (contents.items[i].sessionStatus == 3) // ABORTED
        {
            colTransferStatusE.setAttribute("class", "aborted-status");
        }
        rowE.appendChild(colTransferStatusE);

        colFilenameE = document.createElement("td");
        colFilenameE.innerHTML = contents.items[i].name;
        rowE.appendChild(colFilenameE);

        colSizeE = document.createElement("td");
        colSizeE.innerHTML = get_size_string(contents.items[i].size);
        rowE.appendChild(colSizeE);

        colDurationE = document.createElement("td");
        colDurationE.innerHTML = get_duration_string(contents.items[i].duration);
        rowE.appendChild(colDurationE);

        colPSEReportE = document.createElement("td");
        if (contents.items[i].sessionStatus == 2) // COMPLETED
        {
            // only completed items have a pse report
            
            if (contents.items[i].pseResult == 1) // passed
            {
                colPSEReportE.innerHTML = "<a href='" + contents.items[i].pseURL + "' style='color:green'>" + 
                    "Passed</a>";
            }
            else if (contents.items[i].pseResult == 2) // failed
            {
                colPSEReportE.innerHTML = "<a href='" + contents.items[i].pseURL + "' style='color:red'>" + 
                    "FAILED</a>";
            }
            else // result is not known
            {
                colPSEReportE.innerHTML = "<a href='" + contents.items[i].pseURL + "'>" + 
                    "Result?</a>";
            }
        }
        rowE.appendChild(colPSEReportE);

        colPlayE = document.createElement("td");
        if (contents.items[i].sessionStatus == 2) // COMPLETED
        {
            // only completed items can be played
            var colPlayButtonE = document.createElement("input");
            colPlayButtonE.setAttribute("type", "image");
            colPlayButtonE.setAttribute("src", "images/play_symbol.png");
            colPlayButtonE.setAttribute("title", "play file");
            colPlayButtonE.setAttribute("onclick", "play_file('" + contents.items[i].name + "')");
            colPlayButtonE.setAttribute("onmousedown", "item_control_down(this)");
            colPlayButtonE.setAttribute("onmouseup", "item_control_up(this)");
            colPlayE.appendChild(colPlayButtonE);
        }
        rowE.appendChild(colPlayE);
        
        
        newCacheContentsE.appendChild(rowE);
    }
        
    cacheContentsE.parentNode.replaceChild(newCacheContentsE, cacheContentsE);

}

function cache_contents_handler()
{
    try
    {
        if (cacheContentsRequest.readyState == 4)
        {
            if (cacheContentsRequest.status != 200)
            {
                throw "failed to get cache contents";
            }
            
            var contents = eval("(" + cacheContentsRequest.responseText + ")");
            update_tag_value("cache-path", contents.path, escape(contents));
            set_cache_contents(contents);
            
            cacheStatusChangeCount = contents.statusChangeCount;
            enableCacheContentsRequest = true;
        }
    } 
    catch (err)
    {
        enableCacheContentsRequest = true;
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
            
            update_replay_state(status.replayActive, status.replayFilename);
            
            if (status.statusChangeCount != cacheStatusChangeCount)
            {
                // cache was updated
                request_cache_contents();
            }
            
            // set timer for next status request
            statusTimer = setTimeout("request_status()", statusInterval)
        }
    }
    catch (err)
    {
        set_general_status(null, statusInterval);
        
        // set timer for next status request
        statusTimer = setTimeout("request_status()", statusInterval);
    }
}

function play_file_handler()
{
    try
    {
        if (playFileRequest.readyState == 4)
        {
            if (playFileRequest.status != 200)
            {
                throw "play file request not ok";
            }

            enablePlayFileRequest = true;
        }
    }
    catch (err)
    {
        enablePlayFileRequest = true;
    }
}

function request_status()
{
    statusRequest = new XMLHttpRequest();
    statusRequest.open("GET", "/recorder/status.json?cache=true", true);
    statusRequest.onreadystatechange = status_handler;
    statusRequest.send(null);
}

function request_cache_contents()
{
    if (cacheContentsRequest == null || enableCacheContentsRequest)
    {
        enableCacheContentsRequest = false;
        try
        {
            cacheContentsRequest = new XMLHttpRequest();
            cacheContentsRequest.open("GET", "/recorder/cache/contents.json", true);
            cacheContentsRequest.onreadystatechange = cache_contents_handler;
            cacheContentsRequest.send(null);
        }
        catch (err)
        {
            enableCacheContentsRequest = true;
        }
    }
}

function play_file(name)
{
    if (playFileRequest == null || enablePlayFileRequest)
    {
        enablePlayFileRequest = false;
        try
        {
            playFileRequest = new XMLHttpRequest();
            playFileRequest.open("GET", "/recorder/replay?name=" + encodeURIComponent(name), true);
            playFileRequest.onreadystatechange = play_file_handler;
            playFileRequest.send(null);
        }
        catch (err)
        {
            enablePlayFileRequest = true;
        }
    }
}

function init_cache()
{
    // get status information from the server
    request_status();
}


