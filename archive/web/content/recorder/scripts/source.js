var statusRequest = null;
var sourceInfoRequest = null;
var enableSourceInfoRequest = true; 
var selectDigibetaRequest = null;
var enableSelectDigibetaRequest = true; 
var startNewSessionRequest = null;
var enableStartNewSessionRequest = true;

var lastBarcodeCount = -1;
var sourceInfoBarcode = null;
var digibetaBarcode = null;

var statusTimer = null;

// a status update is performed every 1/2 second
var statusInterval = 500;


function set_start_record_state(status)
{
    var disableButton = (status == null || 
        sourceInfoBarcode == null || digibetaBarcode == null ||
        !status.readyToRecord ||
        !enableSourceInfoRequest);
    
    update_button_disable("start-session-button", disableButton);
}

function set_start_session_error(result)
{
    var startSessionErrorE = document.getElementById("start-session-error");

    var message = null;
    
    if (result != null && result.error)
    {
        message = "<span style='color:red'>" + "Error:&nbsp;";
        switch (result.errorCode)
        {
            case 1: // SESSION_IN_PROGRESS_FAILURE:
                message += "session is already in progress";
                break;
            case 2: // VIDEO_SIGNAL_BAD_FAILURE:
                message += "video signal is bad";
                break;
            case 3: // AUDIO_SIGNAL_BAD_FAILURE:
                message += "audio signal is bad";
                break;
            case 4: // D3_VTR_CONNECT_FAILURE:
                message += "no D3 VTR connection";
                break;
            case 5: //D3_VTR_REMOTE_LOCKOUT_FAILURE:
                message += "D3 VTR remote lockout";
                break;
            case 6: // NO_D3_TAPE:
                message += "no D3 tape";
                break;
            case 7: // DIGIBETA_VTR_CONNECT_FAILURE:
                message += "no Digibeta VTR connection";
                break;
            case 8: // DIGIBETA_VTR_REMOTE_LOCKOUT_FAILURE:
                message += "digibeta VTR remote lockout";
                break;
            case 9: // NO_DIGIBETA_TAPE:
                message += "no Digibeta tape";
                break;
            case 10: // DISK_SPACE_FAILURE:
                message += "not enough disk space";
                break;
            case 11: // MULTI_ITEM_MXF_EXISTS_FAILURE:
                message += "multi-item MXF files already in cache";
                break;
            case 12: // MULTI_ITEM_NOT_ENABLED_FAILURE:
                message += "multi-item ingest not enabled";
                break;
            default:
                // unknown code - use the supplied message
                message += result.errorMessage;
        }
        message += "</span>";
    }
        
    startSessionErrorE.innerHTML = message;
}

function set_digibeta_status(status)
{
    var digibetaStatusE = document.getElementById("digibeta-status");

    var message;
    
    if (status == null)
    {
        message = "No Digibeta selected";
    }
    else if (status.error)
    {
        message = "<span style='color:red'>" + "Error:&nbsp;";
        switch (status.errorCode)
        {
            case 0:
                message += "barcode is not a digibeta barcode";
                break;
            default:
                // unknown code - use the supplied message
                message += status.errorMessage;
        }
        message += "</span>";
    }
    else
    {
        message = "Selected digibeta <span style='color:blue'>" + status.barcode.replace(/\ /g, "&nbsp;") + ".</span>";
        if (status.warningCode != null)
        {
            message += "<br/><span style='color:red'>Warning:&nbsp;";
            switch (status.warningCode)
            {
                case 0:
                    message += "digibeta has been used before";
                    break;
                default:
                    // unknown code - use the supplied message
                    message += status.warningMessage;
            }
            message += "</span>";
        }
    }

    digibetaStatusE.innerHTML = message;
}

function set_source_item_value(rowE, name, colSpan, value)
{
    var colE = document.createElement("td");
    colE.setAttribute("class", "item-heading");
    colE.innerHTML = name;
    rowE.appendChild(colE);
    colE = document.createElement("td");
    if (colSpan > 1)
    {
        colE.setAttribute("colspan", colSpan);
    }
    colE.innerHTML = value;
    rowE.appendChild(colE);
}

function set_source_info(sourceInfo)
{
    var sourceStatusE = document.getElementById("source-status");
    var itemsInfoE = document.getElementById("items-info");

    if (sourceInfo == null)
    {
        document.getElementById("src-spool-no").innerHTML = "";
        document.getElementById("src-format").innerHTML = "";
        document.getElementById("src-stock-date").innerHTML = "";
        document.getElementById("src-total-duration").innerHTML = "";
        document.getElementById("src-spool-status").innerHTML = "";

        itemsInfoE.innerHTML = "";
        
        sourceInfoBarcode = null;
        sourceStatusE.innerHTML = "No source selected.";
        focus_source_input();
        return;
    }
    
    if (sourceInfo.error)
    {
        document.getElementById("src-spool-no").innerHTML = "";
        document.getElementById("src-format").innerHTML = "";
        document.getElementById("src-stock-date").innerHTML = "";
        document.getElementById("src-total-duration").innerHTML = "";
        document.getElementById("src-spool-status").innerHTML = "";

        itemsInfoE.innerHTML = "";
        
        sourceInfoBarcode = null;

        var message = "<span style='color:red'>" + "Error:&nbsp;";
        switch (sourceInfo.errorCode)
        {
            case 0:
                message += "barcode is an empty string";
                break;
            case 1:
                message += "unknown source";
                break;
            case 2:
                message += "multi-item ingest not enabled";
                break;
            default:
                // unknown code - use the supplied message
                message += sourceInfo.errorMessage;
        }
        message += "</span>";
        sourceStatusE.innerHTML = message;
        
        focus_source_input();
        return;
    }

    
    // set the source status message and barcode
    
    var message;
    if (sourceInfo.items.length > 1)
    {
        message = "Selected <span style=\"color:blue\">" + 
            sourceInfo.barcode.replace(/\ /g, "&nbsp;") + " (" + sourceInfo.items.length + " items)</span>";
    }
    else
    {
        message = "Selected <span style=\"color:blue\">" + 
            sourceInfo.barcode.replace(/\ /g, "&nbsp;") + "</span>";
    }
    if (sourceInfo.warningCode != null)
    {
        message += "<br/><span style='color:red'>Warning:&nbsp;";
        switch (sourceInfo.warningCode)
        {
            case 0:
                message += "D3 has been ingested before";
                break;
            default:
                // unknown code - use the supplied message
                message += sourceInfo.warningMessage;
        }
        message += "</span>";
    }
    
    sourceStatusE.innerHTML = message;
    sourceInfoBarcode = sourceInfo.barcode;

    
    // tape info
    
    document.getElementById("src-spool-no").innerHTML = sourceInfo.tapeInfo.spoolNo.replace(/\ /g, "&nbsp;");
    document.getElementById("src-format").innerHTML = sourceInfo.tapeInfo.format;
    document.getElementById("src-stock-date").innerHTML = sourceInfo.tapeInfo.stockDate;
    document.getElementById("src-total-duration").innerHTML = get_duration_string(sourceInfo.tapeInfo.totalInfaxDuration);
    document.getElementById("src-spool-status").innerHTML = sourceInfo.tapeInfo.spoolStatus;


    // items

    itemsInfoE.innerHTML = "";
    
    var rowE;
    var colE;
    var newItemInfoTableE;
    var i;
    for (i = 0; i < sourceInfo.items.length; i++)
    {
        var newItemInfoTableE = document.createElement("table");
        newItemInfoTableE.setAttribute("class", "source-info-table");
        
        // head
        var headE = document.createElement("thead")
        rowE = document.createElement("tr");
        colE = document.createElement("th");
        colE.setAttribute("colspan", 6);
        colE.innerHTML = "Item " + (i + 1) + " of " + sourceInfo.items.length;
        rowE.appendChild(colE);
        headE.appendChild(rowE);
        newItemInfoTableE.appendChild(headE);
    
        // body
        var bodyE = document.createElement("tbody")

        rowE = document.createElement("tr");
        set_source_item_value(rowE, "Prog No", 1, sourceInfo.items[i].progNo);
        set_source_item_value(rowE, "Item No", 1, sourceInfo.items[i].itemNo);
        set_source_item_value(rowE, "Infax Duration", 1, get_duration_string(sourceInfo.items[i].infaxDuration));
        bodyE.appendChild(rowE);

        rowE = document.createElement("tr");
        set_source_item_value(rowE, "Prog Title", 3, sourceInfo.items[i].progTitle);
        set_source_item_value(rowE, "Tx Date", 1, sourceInfo.items[i].txDate);
        bodyE.appendChild(rowE);

        rowE = document.createElement("tr");
        set_source_item_value(rowE, "Episode Title", 3, sourceInfo.items[i].episodeTitle);
        set_source_item_value(rowE, "Spool Descr", 1, sourceInfo.items[i].spoolDescr);
        bodyE.appendChild(rowE);

        rowE = document.createElement("tr");
        set_source_item_value(rowE, "Memo", 5, sourceInfo.items[i].memo);
        bodyE.appendChild(rowE);


        newItemInfoTableE.appendChild(bodyE);
        
        itemsInfoE.appendChild(newItemInfoTableE);
    }
        
    focus_digibeta_input();
}

function source_info_handler()
{
    try
    {
        if (sourceInfoRequest.readyState == 4)
        {
            if (sourceInfoRequest.status != 200)
            {
                throw "source info request status not ok";
            }
            
            var sourceInfo = eval("(" + sourceInfoRequest.responseText + ")");
            set_source_info(sourceInfo);
            set_start_session_error(null);
            
            enableSourceInfoRequest = true;
        }
    } 
    catch (err)
    {
        set_source_info(null);
        enableSourceInfoRequest = true;
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

            // vtr states           
            update_tag_value("d3-vtr-state", status.d3VTRState, status.d3VTRState);
            update_tag_value("digibeta-vtr-state", status.digibetaVTRState, status.digibetaVTRState);
            
            // process barcode
            if (status.hasOwnProperty("barcode") && status.hasOwnProperty("barcodeCount"))
            {
                if (status.barcodeCount != lastBarcodeCount) // differ if a new barcode has been scanned
                {
                    if ((document.forms.digibetabarcodeform.digibetabarcodeinput.hasOwnProperty("hasFocus") &&
                            document.forms.digibetabarcodeform.digibetabarcodeinput.hasFocus) ||
                        status.barcode.charAt(0) == 'C') // we assume d3 barcodes never start with a 'C'
                    {
                        // Digibeta barcode
                        document.forms.digibetabarcodeform.digibetabarcodeinput.focus();
                        document.forms.digibetabarcodeform.digibetabarcodeinput.value = status.barcode;
                        if (status.barcode != digibetaBarcode)
                        {
                            select_digibeta(status.barcode);
                        }
                    }
                    else
                    {
                        // Source barcode
                        document.forms.sourcebarcodeform.sourcebarcodeinput.focus();
                        document.forms.sourcebarcodeform.sourcebarcodeinput.value = status.barcode;
                        if (status.barcode != sourceInfoBarcode)
                        {
                            request_source_info(status.barcode);
                        }
                    }

                    lastBarcodeCount = status.barcodeCount;
                }
            }
            
            // set the start_record state
            set_start_record_state(status);
    
            // set timer for next status request
            statusTimer = setTimeout("request_status()", statusInterval)
        }
    }
    catch (err)
    {
        set_general_status(null, statusInterval);

        // vtr states
        update_tag_value("d3-vtr-state", "", 0);
        update_tag_value("digibeta-vtr-state", "", 0);
        
        // set the start_record state
        set_start_record_state(null);

        // set timer for next status request
        statusTimer = setTimeout("request_status()", statusInterval);
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
                set_start_session_error(result);
                enableStartNewSessionRequest = true;
                return;
            }

            // redirect to the record page
            location.href = "record.html";

            enableStartNewSessionRequest = true;
        }
    } 
    catch (err)
    {
        enableStartNewSessionRequest = true;
        
        var result = new Object;
        result.error = true;
        result.barcode = "";
        result.errorCode = -1;
        result.errorMessage = "server request failed";
        set_start_session_error(result);
    }
}

function autocorrect_d3_barcode(inputField)
{
    // make uppercase
    inputField.value = inputField.value.toUpperCase();
    
    var barcodeArr = inputField.value.split("");
    
    // spool number is 3 letters followed by 6 digits
    // the letters are suffix padded with spaces
    // letters are capitalised

    if (barcodeArr.length == 9)
    {
        // assume barcode is ok
        return;
    }
    
    if (barcodeArr.length < 2 || barcodeArr.length > 9) 
    {
        // Invalid barcode
        return;
    }
    if (!(barcodeArr[0] >= 'A' && barcodeArr[0] <= 'Z'))
    {
        // Invalid barcode
        return;
    }

    if (barcodeArr[1] != ' ' && !(barcodeArr[1] >= 'A' && barcodeArr[1] <= 'Z'))
    {
        barcodeArr.splice(1, 0, " ");
        barcodeArr.splice(1, 0, " ");
    }
    else if (barcodeArr.length < 3)
    {
        // invalid barcode
        return;
    }
    else if (barcodeArr[2] != ' ' && !(barcodeArr[2] >= 'A' && barcodeArr[2] <= 'Z'))
    {
        barcodeArr.splice(2, 0, " ");
    }

    while (barcodeArr.length < 9)
    {
        barcodeArr.splice(3, 0, "0");
    }

    if (barcodeArr.length != 9)
    {
        // Invalid barcode
        return;
    }
    
    inputField.value = barcodeArr.join("");
}

function select_d3(inputField)
{
    autocorrect_d3_barcode(inputField);
    
    request_source_info(inputField.value);
}

function request_source_info(barcode)
{
    if (sourceInfoRequest != null && !enableSourceInfoRequest)
    {
        // busy with previous request
        return;
    }

    enableSourceInfoRequest = false;

    
    // check locally that the barcode is not an LTO tape barcode
    if (barcode == null || barcode.length == 0)
    {
        enableSourceInfoRequest = true;
        set_source_info(null);
        return;
    }
    if (barcode.substr(0, 3).toUpperCase() == "LTA")
    {
        var sourceInfo = new Object();
        sourceInfo.error = true;
        sourceInfo.barcode = barcode;
        sourceInfo.errorCode = -1; // force the use of the errorMessage
        sourceInfo.errorMessage = "barcode is an LTO barcode";
        
        enableSourceInfoRequest = true;
        set_source_info(sourceInfo);
        return;
    }

    
    try
    {
        sourceInfoRequest = new XMLHttpRequest();
        sourceInfoRequest.open("GET", "/recorder/sourceinfo.json?barcode=" + encodeURIComponent(barcode), true);
        sourceInfoRequest.onreadystatechange = source_info_handler;
        sourceInfoRequest.send(null);
    }
    catch (err)
    {
        enableSourceInfoRequest = true;
    }
}

function request_status()
{
    statusRequest = new XMLHttpRequest();
    statusRequest.open("GET", "/recorder/status.json?barcode=true", true);
    statusRequest.onreadystatechange = status_handler;
    statusRequest.send(null);
}

function start_new_session()
{
    var srcBarcode = sourceInfoBarcode;
    var dbBarcode = digibetaBarcode;
    
    if (srcBarcode != null && srcBarcode.length != 0 && 
        dbBarcode != null && dbBarcode.length != 0 &&
        (startNewSessionRequest == null || enableStartNewSessionRequest))
    {
        enableStartNewSessionRequest = false;
        try
        {
            startNewSessionRequest = new XMLHttpRequest();
            startNewSessionRequest.open("GET", "/recorder/newsession.json?barcode=" + 
                encodeURIComponent(srcBarcode) + "&digibetabarcode=" + encodeURIComponent(dbBarcode), true);
            startNewSessionRequest.onreadystatechange = start_new_session_handler;
            startNewSessionRequest.send(null);
        }
        catch (err)
        {
            enableStartNewSessionRequest = true;
        }
    }
}

function focus_source_input()
{
    document.forms.sourcebarcodeform.sourcebarcodeinput.focus();
}

function focus_digibeta_input()
{
    document.forms.digibetabarcodeform.digibetabarcodeinput.focus();
}

function clear_source_input()
{
    document.forms.sourcebarcodeform.sourcebarcodeinput.value = "";
    document.forms.sourcebarcodeform.sourcebarcodeinput.focus();
}

function clear_digibeta_input()
{
    document.forms.digibetabarcodeform.digibetabarcodeinput.value = "";
    document.forms.digibetabarcodeform.digibetabarcodeinput.focus();
}

function select_digibeta_handler()
{
    try
    {
        if (selectDigibetaRequest.readyState == 4)
        {
            if (selectDigibetaRequest.status != 200)
            {
                throw "select digibeta request status not ok";
            }
            
            var result = eval("(" + selectDigibetaRequest.responseText + ")");
            
            if (result.error)
            {
                digibetaBarcode = null;
                set_digibeta_status(result);
                focus_digibeta_input();
                enableSelectDigibetaRequest = true;
                return;
            }

            digibetaBarcode = result.barcode;
            set_digibeta_status(result);
            focus_source_input();
            
            enableSelectDigibetaRequest = true;
        }
    } 
    catch (err)
    {
        digibetaBarcode = null;
        enableSelectDigibetaRequest = true;
        set_digibeta_status(null);
    }
}

function autocorrect_digibeta_barcode(inputField)
{
    // make uppercase
    inputField.value = inputField.value.toUpperCase();
}

function select_digibeta(inputField)
{
    if (selectDigibetaRequest != null && !enableSelectDigibetaRequest)
    {
        return;
    }

    enableSelectDigibetaRequest = false;
    
    autocorrect_digibeta_barcode(inputField);
    
    var barcode = inputField.value;
    if (barcode == null || barcode.length == 0)
    {
        digibetaBarcode = null;
        enableSelectDigibetaRequest = true;
        set_digibeta_status(null);
        return;
    }
    
    try
    {
        selectDigibetaRequest = new XMLHttpRequest();
        selectDigibetaRequest.open("GET", "/recorder/checkselecteddigibeta.json?barcode=" + encodeURIComponent(barcode), true);
        selectDigibetaRequest.onreadystatechange = select_digibeta_handler;
        selectDigibetaRequest.send(null);
    }
    catch (err)
    {
        enableSelectDigibetaRequest = true;
    }
}

function init_source()
{
    // clear the barcode input text and set focus
    clear_digibeta_input();
    clear_source_input();
    
    // get status information from the server
    request_status();
}


