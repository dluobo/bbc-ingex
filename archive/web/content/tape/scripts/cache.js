var cacheContentsRequest = null;
var enableCacheContentsRequest = true;
var cacheStatusChangeCount = -1;
var deleteItemsRequest = null;
var enableDeleteItemsRequest = true;

// a status update is performed every 1/2 second
var statusInterval = 500;


function check_button(ele)
{
    ele.setAttribute("class", "checked-button");
}

function uncheck_button(ele)
{
    ele.setAttribute("class", "unchecked-button");
}

function is_checked(ele)
{
    return ele.getAttribute("class") == "checked-button";
}


function update_selected_items_stats()
{
    var rows = document.getElementsByName("select-item");
    if (rows == null || rows.length == 0)
    {
        return;
    }

    var numItems = 0;
    var totalSize = 0;
    var rowIndex;
    for (rowIndex = 0; rowIndex < rows.length; rowIndex++)
    {
        if (is_checked(rows[rowIndex]))
        {
            numItems++;
            if (rows[rowIndex].hasOwnProperty("itemSize"))
            {
                totalSize += rows[rowIndex].itemSize
            }
        }
    }
    
    update_tag_value("num-selected_items", numItems, numItems);
    update_tag_value("total-size", get_size_string(totalSize), totalSize);
    
    manualTotalSize = totalSize;
}

function set_cache_contents(contents)
{
    var cacheContentsE = document.getElementById("cache-contents");

    var newCacheContentsE = document.createElement("table");
    newCacheContentsE.id = "cache-contents";
    
    // headings
    var rowE = document.createElement("tr");
    rowE.setAttribute("style", "color:blue");
    
    var colSelectE = document.createElement("td");
    rowE.appendChild(colSelectE);
    var checkBoxE;
    
    var colSpoolNoE = document.createElement("td");
    colSpoolNoE.innerHTML = "Spool No";
    rowE.appendChild(colSpoolNoE);
    
    var colItemNoE = document.createElement("td");
    colItemNoE.innerHTML = "Item No";
    rowE.appendChild(colItemNoE);
    
    var colProgNoE = document.createElement("td");
    colProgNoE.innerHTML = "Prog No";
    rowE.appendChild(colProgNoE);
    
    var colDateRecordedE = document.createElement("td");
    colDateRecordedE.innerHTML = "Date Recorded";
    rowE.appendChild(colDateRecordedE);
    
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
    
    newCacheContentsE.appendChild(rowE);
    
    
    // items
    for (i = 0; i < contents.items.length; i++)
    {
        rowE = document.createElement("tr");
        
        colSelectE = document.createElement("td");
        colSelectE.setAttribute("class", "check-button-col");
        if (contents.items[i].sessionStatus != 1 && // STARTED items cannot be accessed
            !contents.items[i].locked) // only items not used in a transfer session can be selected for deletion
        {
            checkBoxE = document.createElement("input");
            checkBoxE.setAttribute("type", "button");
            checkBoxE.setAttribute("name", "select-item");
            checkBoxE.setAttribute("itemId", contents.items[i].identifier);
            checkBoxE.setAttribute("onclick", "toggle_item_select(this)");
            uncheck_button(checkBoxE);
            checkBoxE.itemSize = contents.items[i].size; // used to calculate total size of selected items
            colSelectE.appendChild(checkBoxE);
        }
        rowE.appendChild(colSelectE);
    
        colSpoolNoE = document.createElement("td");
        colSpoolNoE.innerHTML = contents.items[i].srcSpoolNo.replace(/\ /g, "&nbsp;");
        rowE.appendChild(colSpoolNoE);

        colItemNoE = document.createElement("td");
        colItemNoE.innerHTML = contents.items[i].srcItemNo;
        rowE.appendChild(colItemNoE);

        colProgNoE = document.createElement("td");
        colProgNoE.innerHTML = contents.items[i].srcMPProgNo;
        rowE.appendChild(colProgNoE);

        colDateRecordedE = document.createElement("td");
        colDateRecordedE.innerHTML = contents.items[i].sessionCreation;
        rowE.appendChild(colDateRecordedE);

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
        rowE.appendChild(colPSEReportE);

        
        newCacheContentsE.appendChild(rowE);
    }
        
    cacheContentsE.parentNode.replaceChild(newCacheContentsE, cacheContentsE);

    update_selected_items_stats();
}

function delete_items_handler()
{
    try
    {
        if (cacheContentsRequest.readyState == 4)
        {
            if (cacheContentsRequest.status != 200)
            {
                throw "failed to delete items";
            }
            
            request_cache_contents();
            
            enableDeleteItemsRequest = true;
        }
    } 
    catch (err)
    {
        enableDeleteItemsRequest = true;
        // ignore failure
    }
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

function request_status()
{
    statusRequest = new XMLHttpRequest();
    statusRequest.open("GET", "/tape/status.json?cache=true", true);
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
            cacheContentsRequest.open("GET", "/tape/cache/contents.json", true);
            cacheContentsRequest.onreadystatechange = cache_contents_handler;
            cacheContentsRequest.send(null);
        }
        catch (err)
        {
            enableCacheContentsRequest = true;
        }
    }
}

function delete_items()
{
    // set a safe keyboard focus
    set_focus("cache-link");
    
    if (deleteItemsRequest == null || enableDeleteItemsRequest)
    {
        enableDeleteItemsRequest = false;
        try
        {
            // get the cache items
            var rows = document.getElementsByName("select-item");
            if (rows == null || rows.length == 0)
            {
                // no rows to delete
                enableDeleteItemsRequest = true;
                return;
            }
            
            // construct the selected items POST data
            var selectedItems = new Array();
            var rowIndex;
            for (rowIndex = 0; rowIndex < rows.length; rowIndex++)
            {
                if (is_checked(rows[rowIndex]))
                {
                    selectedItems.push(rows[rowIndex].getAttribute("itemId"));
                }
            }
            
            if (selectedItems.length == 0)
            {
                // no rows selected for deletion
                enableDeleteItemsRequest = true;
                return;
            }
            
            // get user confirmation
            if (!confirm("Please confirm deletion of " + selectedItems.length + " items"))
            {
                enableDeleteItemsRequest = true;
                return
            }
            
            deleteItemsRequest = new XMLHttpRequest();
            deleteItemsRequest.open("POST", "/tape/cache/deleteitems", true);
            deleteItemsRequest.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');    
            deleteItemsRequest.onreadystatechange = delete_items_handler;
            deleteItemsRequest.send("items=" + encodeURIComponent(selectedItems.join(",")));
        }
        catch (err)
        {
            enableDeleteItemsRequest = true;
        }
    }
}

function toggle_item_select(ele)
{
    if (is_checked(ele))
    {
        uncheck_button(ele);
    }
    else
    {
        check_button(ele);
    }
    
    update_selected_items_stats();
}

function init_cache()
{
    // set a safe keyboard focus
    set_focus("cache-link");
    
    // get status information from the server
    request_status();
}


