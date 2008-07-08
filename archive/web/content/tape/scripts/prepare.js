var cacheContentsRequest = null;
var enableCacheContentsRequest = true;
var cacheStatusChangeCount = -1;
var setPrepareMethodRequest = null;
var enableSetPrepareMethodRequest = true;
var checkSelectionRequest = null;
var enableCheckSelectionRequest = true;
var startNewSessionRequest = null;
var enableStartNewSessionRequest = true;

var lastBarcodeCount = -1;
var tapeBarcode = null;

var maxAutoFiles = 0;
var maxTotalSize = 0;

var manualTotalSize = 0;


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


function set_start_transfer_state(status)
{
    var manualDisableButton = (status == null || 
        tapeBarcode == null || 
        !status.readyToExport || 
        manualTotalSize == 0 || manualTotalSize > maxTotalSize);

    update_button_disable("start-manual-transfer-button", manualDisableButton);

    
    var automaticDisableButton = (status == null || 
        tapeBarcode == null || 
        !status.readyToExport);
    
    update_button_disable("start-automatic-transfer-button", automaticDisableButton);
}

function set_start_automatic_transfer_error(message, barcode)
{
    var startTransferErrorE = document.getElementById("start-automatic-transfer-message");
    if (startTransferErrorE == null)
    {
        return;
    }
    
    if (message == null)
    {
        startTransferErrorE.innerHTML = null;
    }
    else
    {
        startTransferErrorE.innerHTML = "<span style=\"color:red\">" + "Error: " + message  + 
            " (" + ((barcode == null) ? "?" : barcode) + ")</span>";
    }
}

function set_start_manual_transfer_error(message, barcode)
{
    var startTransferErrorE = document.getElementById("start-manual-transfer-message");
    if (startTransferErrorE == null)
    {
        return;
    }
    
    if (message == null)
    {
        startTransferErrorE.innerHTML = null;
    }
    else
    {
        startTransferErrorE.innerHTML = "<span style=\"color:red\">" + "Error: " + message  + 
            " (" + ((barcode == null) ? "?" : barcode) + ")</span>";
    }
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
            // TODO: will it ever not have an itemSize property?
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

function set_cache_contents_visibility(hidden)
{
    // clear the contents
    var cacheContentsE = document.getElementById("cache-contents");
    var selectItemsE = document.getElementById("select-items");
    if (cacheContentsE == null || selectItemsE == null)
    {
        return;
    }
    var newCacheContentsE = document.createElement("table");
    newCacheContentsE.id = "cache-contents";
    cacheContentsE.parentNode.replaceChild(newCacheContentsE, cacheContentsE);


    // set the visibility    
    if (hidden)
    {
        selectItemsE.setAttribute("style", "visibility:hidden");
    }
    else
    {
        selectItemsE.setAttribute("style", null);
    }
}

function set_cache_contents(contents)
{
    var cacheContentsE = document.getElementById("cache-contents");
    if (cacheContentsE == null)
    {
        return;
    }

    // store the current item selection
    var selectedItemsMap = new Array();    
    var rows = document.getElementsByName("select-item");
    if (rows != null && rows.length != 0)
    {
        var rowIndex;
        for (rowIndex = 0; rowIndex < rows.length; rowIndex++)
        {
            if (is_checked(rows[rowIndex]))
            {
                selectedItemsMap[rows[rowIndex].getAttribute("itemId")] = true;
            }
        }
    }

    
    
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
    
    newCacheContentsE.appendChild(rowE);
    
    
    // items
    for (i = 0; i < contents.items.length; i++)
    {
        rowE = document.createElement("tr");
        
        colSelectE = document.createElement("td");
        colSelectE.setAttribute("class", "check-button-col");
        if (contents.items[i].sessionStatus != 1 && // STARTED items cannot be accessed
            !contents.items[i].locked) // only items not used in a transfer session can be selected for transfer
        {
            checkBoxE = document.createElement("input");
            checkBoxE.setAttribute("type", "button");
            checkBoxE.setAttribute("name", "select-item");
            checkBoxE.setAttribute("onclick", "toggle_item_select(this)");
            checkBoxE.setAttribute("itemId", contents.items[i].identifier);
            checkBoxE.itemSize = contents.items[i].size; // used to calculate total size of selected items
            if (selectedItemsMap[contents.items[i].identifier])
            {
                check_button(checkBoxE);
            }
            else
            {
                uncheck_button(checkBoxE);
            }
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

        
        newCacheContentsE.appendChild(rowE);
    }
        
    cacheContentsE.parentNode.replaceChild(newCacheContentsE, cacheContentsE);
    
    update_selected_items_stats();
}

function select_manual_transfer_method()
{
    var manualPrepareMethodE = document.getElementById("manual-transfer-method");
    var automaticPrepareMethodE = document.getElementById("automatic-transfer-method");

    if (manualPrepareMethodE == null || automaticPrepareMethodE == null)
    {
        return;
    }
    
    manualPrepareMethodE.setAttribute("class", null);
    automaticPrepareMethodE.setAttribute("class", "hidediv");
    update_element_class("select-manual-transfer-method", "active-form-button");
    update_element_class("select-automatic-transfer-method", "inactive-form-button");
    set_cache_contents_visibility(false);
    set_start_manual_transfer_error(null, null);
    request_cache_contents();
}

function select_automatic_transfer_method()
{
    var manualPrepareMethodE = document.getElementById("manual-transfer-method");
    var automaticPrepareMethodE = document.getElementById("automatic-transfer-method");

    if (manualPrepareMethodE == null || automaticPrepareMethodE == null)
    {
        return;
    }
    
    manualPrepareMethodE.setAttribute("class", "hidediv");
    automaticPrepareMethodE.setAttribute("class", null);
    update_element_class("select-automatic-transfer-method", "active-form-button");
    update_element_class("select-manual-transfer-method", "inactive-form-button");
    set_cache_contents_visibility(true);
    set_start_automatic_transfer_error(null, null);
}

function set_selection_status(selectionStatus)
{
    var selectedBarcodeE = document.getElementById("selection-status");
    if (selectedBarcodeE == null)
    {
        return;
    }
    
    // reset the transfer error
    set_start_automatic_transfer_error(null, null);
    set_start_manual_transfer_error(null, null);

    if (selectionStatus == null)
    {
        selectedBarcodeE.innerHTML = "No tape selected.";
        tapeBarcode = null;
        return;
    }

    if (selectionStatus.accepted)
    {
        selectedBarcodeE.innerHTML = "Selected LTO tape <span style=\"color:blue\">" + 
            selectionStatus.barcode + "</span>";
        tapeBarcode = selectionStatus.barcode;
    }
    else
    {
        selectedBarcodeE.innerHTML = "<span style=\"color:red\">Error: " + selectionStatus.message  + 
            " (" + ((selectionStatus.barcode == null) ? "?" : selectionStatus.barcode) + ")</span>";
        tapeBarcode = null;
    }
}

function start_new_session_handler()
{
    try
    {
        if (startNewSessionRequest.readyState == 4)
        {
            if (startNewSessionRequest.status != 200)
            {
                throw "source info request status not ok";
            }
            
            var result = eval("(" + startNewSessionRequest.responseText + ")");
            
            if (result.error)
            {
                if (startNewSessionRequest.automatic)
                {
                    set_start_automatic_transfer_error(result.errorMessage, result.barcode);
                    enableStartNewSessionRequest = true;
                    return;
                }
                else
                {
                    set_start_manual_transfer_error(result.errorMessage, result.barcode);
                    enableStartNewSessionRequest = true;
                    return;
                }
            }

            // redirect to the recording page
            location.href = result.redirect;

            enableStartNewSessionRequest = true;
        }
    } 
    catch (err)
    {
        enableStartNewSessionRequest = true;
        set_start_automatic_transfer_error("Server request failed", null);
        set_start_manual_transfer_error("Server request failed", null);
    }
}

function check_selection_handler()
{
    try
    {
        if (checkSelectionRequest.readyState == 4)
        {
            if (checkSelectionRequest.status != 200)
            {
                throw "check selection request status not ok";
            }
            
            var selectionStatus = eval("(" + checkSelectionRequest.responseText + ")");
            set_selection_status(selectionStatus);
            
            enableCheckSelectionRequest = true;
        }
    } 
    catch (err)
    {
        var selectionStatus = new Object();
        selectionStatus.accepted = false;
        selectionStatus.barcode = checkSelectionRequest.hasOwnProperty("barcode") ? checkSelectionRequest.barcode : "";
        selectionStatus.message = "Failed to check barcode with server";
        set_selection_status(selectionStatus);
        
        enableCheckSelectionRequest = true;
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

            maxAutoFiles = status.maxFiles;
            maxTotalSize = status.maxTotalSize;
            
            // update tape status
            var statusString = get_tape_dev_status_string(status.tapeDevStatus, status.tapeDevDetailedStatus);
            update_tag_value("tape-state", statusString, statusString);
            
            // process barcode
            if (status.hasOwnProperty("barcode") && status.hasOwnProperty("barcodeCount"))
            {
                if (status.barcode != tapeBarcode && status.barcodeCount != lastBarcodeCount)
                {
                    document.forms.barcodeform.barcodeinput.value = status.barcode;
                    lastBarcodeCount = status.barcodeCount;
                    check_selection(status.barcode);
                }
            }

            if (status.statusChangeCount != cacheStatusChangeCount)
            {
                // cache was updated
                request_cache_contents();
            }
            
            set_start_transfer_state(status);
            
            // set timer for next status request
            statusTimer = setTimeout("request_status()", statusInterval)
        }
    }
    catch (err)
    {
        set_general_status(null, statusInterval);
        set_start_transfer_state(null);
        update_tag_value("tape-state", "", 0);
        
        // set timer for next status request
        statusTimer = setTimeout("request_status()", statusInterval);
    }
}

function request_status()
{
    statusRequest = new XMLHttpRequest();
    statusRequest.open("GET", "/tape/status.json?barcode=true&cache=true&prepare=true", true);
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

function autocorrect_lto_barcode(inputField)
{
    // make uppercase
    inputField.value = inputField.value.toUpperCase();
}

function check_selection(inputField)
{
    autocorrect_lto_barcode(inputField);
    
    var barcode = inputField.value;
    if (barcode == null || barcode.length == 0)
    {
        set_selection_status(null);
        return;
    }
    
    // check selected barcode
    if (checkSelectionRequest == null || enableCheckSelectionRequest)
    {
        enableCheckSelectionRequest = false;
        try
        {
            checkSelectionRequest = new XMLHttpRequest();
            checkSelectionRequest.barcode = barcode; // used when the server connection fails 
            checkSelectionRequest.open("GET", "/tape/checkselection.json?barcode=" + encodeURIComponent(barcode), true);
            checkSelectionRequest.onreadystatechange = check_selection_handler;
            checkSelectionRequest.send(null);
        }
        catch (err)
        {
            enableCheckSelectionRequest = true;
        }
    }
}

function start_automatic_transfer()
{
    var barcode = tapeBarcode;
    
    if (barcode != null && barcode.length != 0 &&
        (startNewSessionRequest == null || enableStartNewSessionRequest))
    {
        enableStartNewSessionRequest = false;
        try
        {
            startNewSessionRequest = new XMLHttpRequest();
            startNewSessionRequest.open("GET", "/tape/newautosession.json?barcode=" + encodeURIComponent(barcode), true);
            startNewSessionRequest.onreadystatechange = start_new_session_handler;
            startNewSessionRequest.send(null);
        }
        catch (err)
        {
            enableStartNewSessionRequest = true;
        }
    }
}

function start_manual_transfer()
{
    var barcode = tapeBarcode;
    
    if (barcode != null &&
        (startNewSessionRequest == null || enableStartNewSessionRequest))
    {
        enableStartNewSessionRequest = false;
        try
        {
            // get the selected items
            var rows = document.getElementsByName("select-item");
            if (rows == null || rows.length == 0)
            {
                // no items selected
                enableDeleteItemsRequest = true;
                return;
            }
            
            // construct the selected items GET data
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
                // no items selected
                enableDeleteItemsRequest = true;
                return;
            }
            startNewSessionRequest = new XMLHttpRequest();
            startNewSessionRequest.open("GET", "/tape/newmanualsession.json" + 
                "?barcode=" + encodeURIComponent(barcode) + 
                "&items=" + encodeURIComponent(selectedItems.join(",")), true);
            startNewSessionRequest.onreadystatechange = start_new_session_handler;
            startNewSessionRequest.send(null);
        }
        catch (err)
        {
            enableStartNewSessionRequest = true;
        }
    }
}

function auto_select_items()
{
    var rows = document.getElementsByName("select-item");
    if (rows == null || rows.length == 0)
    {
        return;
    }

    // select items with 2 conditions: total size < maxTotalSize and maximum files == maxAutoFiles
    // start from the oldest
    
    var numItems = 0;
    var totalSize = 0;
    var rowIndex;
    for (rowIndex = rows.length - 1; rowIndex >= 0; rowIndex--)
    {
        if (!rows[rowIndex].hasOwnProperty("itemSize"))
        {
            // TODO: will it ever not have an itemSize property?
            continue;
        }
        
        if (totalSize + rows[rowIndex].itemSize > maxTotalSize ||
            numItems >= maxAutoFiles)
        {
            // maximum maxTotalSize per tape
            uncheck_button(rows[rowIndex]);
            break;
        }
        
        check_button(rows[rowIndex]);

        numItems++;
        totalSize += rows[rowIndex].itemSize;
    }

    // uncheck the rest    
    for (; rowIndex >= 0; rowIndex--)
    {
        uncheck_button(rows[rowIndex]);
    }
    
    update_selected_items_stats();
}

function clear_items()
{
    var rows = document.getElementsByName("select-item");
    if (rows == null || rows.length == 0)
    {
        return;
    }
    
    var rowIndex;
    for (rowIndex = 0; rowIndex < rows.length; rowIndex++)
    {
        uncheck_button(rows[rowIndex]);
    }
    
    update_selected_items_stats();
}

function clear_barcode_input()
{
    document.forms.barcodeform.barcodeinput.value = "";
    document.forms.barcodeform.barcodeinput.focus();
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

function init_prepare()
{
    // clear the barcode input text and set focus
    clear_barcode_input();
    
    // get status information from the server
    request_status();
}


