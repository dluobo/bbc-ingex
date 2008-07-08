var statusRequest = null;
var sourceInfoRequest = null;
var changeItemRequest = null;
var itemClipInfoRequest = null;
var pbMarksRequest = null;
var itemMarkRequest = null;
var enableItemRequest = true;

var itemSourceChangeCount = -1;
var itemClipChangeCount = -1;

var itemTables = null;
var items = null;

var movedItemHighlightTimeout = 500;
var itemClipChangedHighlightTimeout = 500;

var sourceInfoSessionState = -1;

var playingItemId = -1;
var playingItemIndex = -1;
var playingItemPosition = -1;

// a status update is performed every x milliseconds
var statusInterval = 500;


// enableItemRequest must be true when calling this function
function update_highlighted_playing_item(itemId)
{
    if (itemTables == null)
    {
        return;
    }
    
    var i;
    for (i = 0; i < itemTables.length; i++)
    {
        if (itemId == parseInt(itemTables[i].getAttribute("item-id")))
        {
            add_class(itemTables[i], "playing");
        }
        else
        {
            remove_class(itemTables[i], "playing");
        }
    }
}

function update_progressbar_marks_handler()
{
    try
    {
        if (pbMarksRequest.readyState == 4)
        {
            if (pbMarksRequest.status != 200)
            {
                throw "item clip info request status not ok";
            }
            
            var itemClipInfo = eval("(" + pbMarksRequest.responseText + ")");
            
            if (pgItemClipChangeCount != itemClipInfo.itemClipChangeCount ||
                pgMarksDuration != pbMarksRequest.duration)
            {
                init_progressbar_marks(itemClipInfo, pbMarksRequest.duration);
            }

            enableItemRequest = true;
        }
    } 
    catch (err)
    {
        enableItemRequest = true;
    }
}

function update_progressbar_marks(duration)
{
    if (enableItemRequest)
    {
        enableItemRequest = false;
        try
        {
            pbMarksRequest = new XMLHttpRequest();
            pbMarksRequest.duration = duration;
            pbMarksRequest.open("GET", "/recorder/session/getitemclipinfo.json", true);
            pbMarksRequest.onreadystatechange = update_progressbar_marks_handler;
            pbMarksRequest.send(null);
        }
        catch (err)
        {
            enableItemRequest = true;
        }
    }
}

function reset_progressbar_marks()
{
    if (enableItemRequest)
    {
        enableItemRequest = false;
        try
        {
            init_progressbar_marks(null, -1);
            enableItemRequest = true;
        }
        catch (err)
        {
            enableItemRequest = true;
        }
    }
}

function sort_item_by_id(l, r)
{
    return l.id - r.id;
}

function update_item_clips(itemClipInfo)
{
    if (itemTables == null || items == null ||
        items.length != itemTables.length ||
        itemClipInfo.items.length != itemTables.length)
    {
        // out of sync
        return false;
    }
    
    // sort clips by id
    itemClipInfo.items.sort(sort_item_by_id);
    
    var durationE;
    var startPositionE;
    var disableEnableButtonE;
    var i;
    for (i = 0; i < itemTables.length; i++)
    {
        if (items[i].itemStartPosition != itemClipInfo.items[i].itemStartPosition ||
            items[i].itemDuration != itemClipInfo.items[i].itemDuration)
        {
            var itemIndex = parseInt(itemTables[i].getAttribute("item-index"));
            
            // update
            items[i].itemStartPosition = itemClipInfo.items[i].itemStartPosition;
            items[i].itemDuration = itemClipInfo.items[i].itemDuration;
            
            playItemE = itemTables[i].firstChild.nextSibling.firstChild.lastChild;
            if ((sourceInfoSessionState == 3 || // 3 == REVIEWING_SESSION_STATE
                    sourceInfoSessionState == 4) && // 4 == PREPARE_CHUNKING_SESSION_STATE
                !items[i].isDisabled && items[i].itemDuration >= 0 &&
                (!items[i].isJunk || sourceInfoSessionState == 4)) // only play chunk in prepare state
            {
                playItemE.innerHTML = "<input " +
                    "class='play-item-control' " +
                    "type='image' " +
                    "src='images/play_item_symbol.png' " +
                    "title='play item' " +
                    "onclick='play_item(" + items[i].id + "," + itemIndex + ")' " +
                    "onmousedown='item_control_down(this)' " +
                    "onmouseup='item_control_up(this)' " +
                    "/>";
            }
            else
            {
                playItemE.innerHTML = "";
            }
            
            durationE = playItemE.previousSibling;
            durationE.innerHTML = get_duration_string(items[i].itemDuration);

            startPositionE = durationE.previousSibling.previousSibling;
            startPositionE.innerHTML = get_position_string(items[i].itemStartPosition);
            
            disableEnableButtonE = startPositionE.previousSibling.previousSibling;
            if (sourceInfoSessionState == 4) // 4 == PREPARE_CHUNKING_SESSION_STATE
            {
                if (itemClipInfo.items[i].isDisabled)
                {
                    disableEnableButtonE.innerHTML = "<input " +
                        "class='enable-item-control' " +
                        "type='image' " +
                        "src='images/enable_item_symbol.png' " +
                        "title='enable item' " +
                        "onclick='enable_item(" + items[i].id + "," + itemIndex + ")' " +
                        "onmousedown='item_control_down(this)' " +
                        "onmouseup='item_control_up(this)' " +
                        "/>";
                }
                else if (itemClipInfo.items[i].itemDuration < 0)
                {
                    disableEnableButtonE.innerHTML = "<input " +
                        "class='disable-item-control' " +
                        "type='image' " +
                        "src='images/disable_item_symbol.png' " +
                        "title='disable item' " +
                        "onclick='disable_item(" + items[i].id + "," + itemIndex + ")' " +
                        "onmousedown='item_control_down(this)' " +
                        "onmouseup='item_control_up(this)' " +
                        "/>";
                }
                else
                {
                    disableEnableButtonE.innerHTML = "";
                }
            }
            else
            {
                disableEnableButtonE.innerHTML = "";
            }
            
            // highlight the changed item
            add_class(itemTables[i], "item-clip-changed");
            setTimeout(remove_class, itemClipChangedHighlightTimeout, itemTables[i], "item-clip-changed");
        }
    }

    update_highlighted_playing_item(playingItemId);
    
    return true;
}

function item_clip_info_handler()
{
    try
    {
        if (itemClipInfoRequest.readyState == 4)
        {
            if (itemClipInfoRequest.status != 200)
            {
                throw "item clip info request status not ok";
            }
            
            var itemClipInfo = eval("(" + itemClipInfoRequest.responseText + ")");
            
            if (itemSourceChangeCount != itemClipInfo.itemSourceChangeCount)
            {
                // source info is out of sync and needs to be reloaded
                return;
            }

            if (!update_item_clips(itemClipInfo))
            {
                // failed to update and will do a reload
                itemClipChangeCount = -1;
                return;
            }
            
            itemClipChangeCount = itemClipInfo.itemClipChangeCount;

            enableItemRequest = true;
        }
    } 
    catch (err)
    {
        itemClipChangeCount = -1;
        enableItemRequest = true;
    }
}

function request_item_clip_info()
{
    if (enableItemRequest)
    {
        enableItemRequest = false;
        try
        {
            itemClipInfoRequest = new XMLHttpRequest();
            itemClipInfoRequest.open("GET", "/recorder/session/getitemclipinfo.json", true);
            itemClipInfoRequest.onreadystatechange = item_clip_info_handler;
            itemClipInfoRequest.send(null);
        }
        catch (err)
        {
            enableItemRequest = true;
        }
    }
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

function create_item_table(item, index, nextItem)
{
    if (itemTables == null || item.id > itemTables.length)
    {
        throw "Cannot create new item table element";
    }
    
    var itemTableE = document.createElement("table");
    add_class(itemTableE, "item-table");
    if (item.isDisabled)
    {
        add_class(itemTableE, "disabled-item-table");
    }
    if (item.isJunk)
    {
        add_class(itemTableE, "junk-item");
    }
    itemTableE.setAttribute("item-index", index);
    itemTableE.setAttribute("item-id", item.id);
    
    
    // table head
    var headE = document.createElement("thead")
    var rowE = document.createElement("tr");
    var colE = document.createElement("th");
    colE.setAttribute("colspan", 8);
    if (item.isJunk)
    {
        colE.innerHTML = "Item " + (index + 1) + " of " + itemTables.length + " <span style='color:red'>(Junk)</span>";
    }
    else
    {
        colE.innerHTML = "Item " + (index + 1) + " of " + itemTables.length;
    }
    rowE.appendChild(colE);
    headE.appendChild(rowE);
    itemTableE.appendChild(headE);

    // table body
    var bodyE = document.createElement("tbody")

    // start position and duration for multiple items
    if (itemTables.length > 1)
    {
        rowE = document.createElement("tr");

        var colE = document.createElement("td");
        colE.setAttribute("class", "chunk-item-info enable-disable-item");
        if (sourceInfoSessionState == 4) // 4 == PREPARE_CHUNKING_SESSION_STATE
        {
            if (item.isDisabled)
            {
                colE.innerHTML = "<input " +
                    "class='enable-item-control' " +
                    "type='image' " +
                    "src='images/enable_item_symbol.png' " +
                    "title='enable item' " +
                    "onclick='enable_item(" + item.id + "," + index + ")' " +
                    "onmousedown='item_control_down(this)' " +
                    "onmouseup='item_control_up(this)' " +
                    "/>";
            }
            else if (item.itemDuration < 0) // only allow disable if clip not set
            {
                colE.innerHTML = "<input " +
                    "class='disable-item-control' " +
                    "type='image' " +
                    "src='images/disable_item_symbol.png' " +
                    "title='disable item' " +
                    "onclick='disable_item(" + item.id + "," + index + ")' " +
                    "onmousedown='item_control_down(this)' " +
                    "onmouseup='item_control_up(this)' " +
                    "/>";
            }
        }
        rowE.appendChild(colE);
        
        colE = document.createElement("td");
        colE.setAttribute("class", "chunk-item-info chunk-item-info-heading");
        colE.innerHTML = "Start Position";
        rowE.appendChild(colE);
        colE = document.createElement("td");
        colE.setAttribute("class", "chunk-item-info");
        colE.setAttribute("colspan", 2);
        colE.innerHTML = get_position_string(item.itemStartPosition);
        rowE.appendChild(colE);

        colE = document.createElement("td");
        colE.setAttribute("class", "chunk-item-info chunk-item-info-heading");
        colE.innerHTML = "Duration";
        rowE.appendChild(colE);
        colE = document.createElement("td");
        colE.setAttribute("class", "chunk-item-info");
        colE.setAttribute("colspan", 2);
        colE.innerHTML = get_duration_string(item.itemDuration);
        rowE.appendChild(colE);
        
        colE = document.createElement("td");
        colE.setAttribute("class", "chunk-item-info play-item-field");
        colE.setAttribute("rowspan", 5);
        if ((sourceInfoSessionState == 3 || // 3 == REVIEWING_CHUNKING_SESSION_STATE
                sourceInfoSessionState == 4) && // 4 == PREPARE_CHUNKING_SESSION_STATE
            !item.isDisabled && item.itemDuration >= 0 &&
            (!item.isJunk || sourceInfoSessionState == 4)) // only play junk in prepare state
        {
            colE.innerHTML = "<input " +
                "class='play-item-control' " +
                "type='image' " +
                "src='images/play_item_symbol.png' " +
                "title='play item' " +
                "onclick='play_item(" + item.id + "," + index + ")' " +
                "onmousedown='item_control_down(this)' " +
                "onmouseup='item_control_up(this)' " +
                "/>";
        }
        rowE.appendChild(colE);

        bodyE.appendChild(rowE);
    }
    
    if (item.isJunk)
    {
        var k;
        for (k = 0; k < 4; k++)
        {
            rowE = document.createElement("tr");

            colE = document.createElement("td");
            colE.setAttribute("colspan", 7);
            colE.innerHTML = "&nbsp;";
            rowE.appendChild(colE);

            bodyE.appendChild(rowE);
        }
    }
    else
    {
        rowE = document.createElement("tr");
    
        // move item up control for multiple items
        if (itemTables.length > 1)
        {
            var colE = document.createElement("td");
            if (sourceInfoSessionState == 4 && // 4 == PREPARE_CHUNKING_SESSION_STATE
                index != 0 && !item.isDisabled)
            {
                colE.innerHTML = "<input " +
                    "class='move-item-control' " +
                    "type='image' " +
                    "src='images/move_item_up_symbol.png' " +
                    "title='move up' " +
                    "onclick='move_item_up(" + item.id + "," + index + ")' " +
                    "onmousedown='item_control_down(this)' " +
                    "onmouseup='item_control_up(this)' " +
                    "/>";
            }
            colE.setAttribute("rowspan", 2);
            colE.setAttribute("class", "chunk-item-info");
            rowE.appendChild(colE);
        }
    
        // item data
        set_source_item_value(rowE, "Prog No", 1, item.progNo);
        set_source_item_value(rowE, "Item No", 1, item.itemNo);
        set_source_item_value(rowE, "Infax Duration", 1, get_duration_string(item.infaxDuration));
        bodyE.appendChild(rowE);
    
        rowE = document.createElement("tr");
    
        // item data
        set_source_item_value(rowE, "Prog Title", 3, item.progTitle);
        set_source_item_value(rowE, "Tx Date", 1, item.txDate);
        bodyE.appendChild(rowE);
    
        rowE = document.createElement("tr");
        
        // move item down control for multiple items
        if (itemTables.length > 1)
        {
            var colE = document.createElement("td");
            if (sourceInfoSessionState == 4 && // 4 == PREPARE_CHUNKING_SESSION_STATE
                index < itemTables.length - 1 && 
                !item.isDisabled && nextItem != null && !nextItem.isDisabled &&
                !item.isJunk && nextItem != null && !nextItem.isJunk)
            {
                colE.innerHTML = "<input " +
                    "class='move-item-control' " + 
                    "type='image' " + 
                    "src='images/move_item_down_symbol.png' " +
                    "title='move down' " +
                    "onclick='move_item_down(" + item.id +  "," + index + ")' " +
                    "onmousedown='item_control_down(this)' " +
                    "onmouseup='item_control_up(this)' " +
                    "/>";
            }
            colE.setAttribute("rowspan", 2);
            colE.setAttribute("class", "chunk-item-info");
            rowE.appendChild(colE);
        }
    
        // item data
        set_source_item_value(rowE, "Episode Title", 3, item.episodeTitle);
        set_source_item_value(rowE, "Spool Descr", 1, item.spoolDescr);
        bodyE.appendChild(rowE);
    
        // item data
        rowE = document.createElement("tr");
        set_source_item_value(rowE, "Memo", 5, item.memo);
        bodyE.appendChild(rowE);
    }

    
    itemTableE.appendChild(bodyE);
    
    itemTables[item.id] = itemTableE;
    items[item.id] = item;
    items[item.id].index = null; // index will be wrong when items are moved; force use of index in itemTables
    
    return itemTableE;
}

function replace_item_table(oldItemTableE, item, itemIndex, nextItem)
{
    var itemWasPlaying = have_class(oldItemTableE, "playing");

    var newItemTableE = create_item_table(item, itemIndex, nextItem);
    oldItemTableE.parentNode.replaceChild(newItemTableE, oldItemTableE);
    
    if (itemWasPlaying)
    {
        add_class(newItemTableE, "playing");
    }
}

function set_source_info(sourceInfo, sessionState)
{
    sourceInfoSessionState = sessionState;
    
    
    var itemsInfoE = document.getElementById("items-info");
    var tapeInfoE = document.getElementById("tape-info");
    
    // reset info
    
    document.getElementById("src-spool-no").innerHTML = "";
    document.getElementById("src-format").innerHTML = "";
    document.getElementById("src-stock-date").innerHTML = "";
    document.getElementById("src-total-duration").innerHTML = "";
    document.getElementById("src-spool-status").innerHTML = "";
    itemsInfoE.innerHTML = "";
    itemTables = null;
    items = null;
    set_tag_state(tapeInfoE, null);
    
    if (sourceInfo == null)
    {
        return;
    }

    
    // tape info
    
    document.getElementById("src-spool-no").innerHTML = sourceInfo.tapeInfo.spoolNo.replace(/\ /g, "&nbsp;");
    document.getElementById("src-format").innerHTML = sourceInfo.tapeInfo.format;
    document.getElementById("src-stock-date").innerHTML = sourceInfo.tapeInfo.stockDate;
    document.getElementById("src-total-duration").innerHTML = get_duration_string(sourceInfo.tapeInfo.totalInfaxDuration);
    document.getElementById("src-spool-status").innerHTML = sourceInfo.tapeInfo.spoolStatus;


    // items
    
    itemsInfoE.innerHTML = "";
    items = null;
    itemTables = null;
    
    itemTables = new Array(sourceInfo.items.length);
    items = new Array(sourceInfo.items.length);
    
    var i;
    for (i = 0; i < sourceInfo.items.length; i++)
    {
        if (i < sourceInfo.items.length - 1)
        {
            itemsInfoE.appendChild(create_item_table(sourceInfo.items[i], i, sourceInfo.items[i + 1]));
        }
        else
        {
            itemsInfoE.appendChild(create_item_table(sourceInfo.items[i], i, null));
        }
    }
    
    update_highlighted_playing_item(playingItemId);
    
    set_tag_state(tapeInfoE, sourceInfo.tapeInfo.spoolNo);
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
            set_source_info(sourceInfo, sourceInfoRequest.sessionState);
            
            itemClipChangeCount = sourceInfo.itemClipChangeCount;
            itemSourceChangeCount = sourceInfo.itemSourceChangeCount;

            enableItemRequest = true;
        }
    } 
    catch (err)
    {
        set_source_info(null, -1);
        enableItemRequest = true;
    }
}

function request_source_info(sessionState)
{
    if (enableItemRequest)
    {
        enableItemRequest = false;
        try
        {
            sourceInfoRequest = new XMLHttpRequest();
            sourceInfoRequest.sessionState = sessionState;
            sourceInfoRequest.open("GET", "/recorder/session/sourceinfo.json", true);
            sourceInfoRequest.onreadystatechange = source_info_handler;
            sourceInfoRequest.send(null);
        }
        catch (err)
        {
            enableItemRequest = true;
        }
    }
}

function set_playing_item_state(state, itemId, itemIndex, itemPos)
{
    if (playingItemId != itemId)
    {
        if (enableItemRequest)
        {
            update_highlighted_playing_item(itemId);
        }
        else
        {
            // reset and wait until the next status update and retry
            playingItemId = -1;
            playingItemIndex = -1;
            playingItemPosition = -1;
            return;
        }
    }
    
    playingItemId = itemId;
    playingItemIndex = itemIndex;
    playingItemPosition = itemPos;
}

function set_session_status(status)
{
    var sourceInfoE = document.getElementById("tape-info");
    
    if (status == null || 
        (status.sessionStatus.state != 1 && // 1 == READY_SESSION_STATE
            status.sessionStatus.state != 2 && // 2 == RECORDING_SESSION_STATE
            status.sessionStatus.state != 3 && // 3 == REVIEWING_SESSION_STATE
            status.sessionStatus.state != 4 && // 4 == PREPARE_CHUNKING_SESSION_STATE
            status.sessionStatus.state != 5)) // 5 == CHUNKING_SESSION_STATE
    {
        if (get_tag_state(sourceInfoE) != null)
        {
            set_source_info(null, -1);
        }
        set_progress_bar_state(0, -1, -1);
        update_progressbar_pointer(-1, -1);
        reset_progressbar_marks();
        set_playing_item_state(-1, -1, -1, -1);
        return;
    }

    set_playing_item_state(status.sessionStatus.state, status.sessionStatus.playingItemId, status.sessionStatus.playingItemIndex, 
        status.sessionStatus.playingItemPosition);
    set_progress_bar_state(status.sessionStatus.state, status.sessionStatus.chunkingPosition, status.sessionStatus.chunkingDuration);
    update_progressbar_pointer(status.sessionStatus.playingFilePosition, status.sessionStatus.playingFileDuration);

    if (get_tag_state(sourceInfoE) != status.sessionStatus.d3SpoolNo ||
        itemClipChangeCount < 0 || itemSourceChangeCount < 0 ||
        status.sessionStatus.itemSourceChangeCount != itemSourceChangeCount)
    {
        request_source_info(status.sessionStatus.state);
        return;
    }

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
        
    if (status.sessionStatus.state != sourceInfoSessionState)
    {
        request_source_info(status.sessionStatus.state);
        return;
    }
    
    if (status.sessionStatus.itemClipChangeCount != itemClipChangeCount)        
    {
        request_item_clip_info();
        return;
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
            
            set_session_status(status);
            
            statusTimer = setTimeout("request_status()", statusInterval)
        }
    }
    catch (err)
    {
        set_session_status(null);
        
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

function html_move_item_down(itemId, itemIndex)
{
    // get the item table, next item table and if present, the next next item table
    var itemTableE = itemTables[itemId];
    var item = items[itemId];
    if (itemTableE == null || parseInt(itemTableE.getAttribute("item-index")) != itemIndex)
    {
        return false;
    }
    var nextItemTableE = itemTableE.nextSibling;
    if (nextItemTableE == null || parseInt(nextItemTableE.getAttribute("item-id")) >= items.length)
    {
        return false;
    }
    var nextItem = items[parseInt(nextItemTableE.getAttribute("item-id"))];
    var nextNextItemTableE = nextItemTableE.nextSibling;
    var nextNextItem = null;
    if (nextNextItemTableE != null && parseInt(nextNextItemTableE.getAttribute("item-id")) < items.length)
    {
        nextNextItem = items[parseInt(nextNextItemTableE.getAttribute("item-id"))];
    }

    
    // store the current page offset for the item so that at the end we move
    // the page so it stays in the same position
    var itemOffsetLeft = itemTableE.offsetLeft;
    var itemOffsetTop = itemTableE.offsetTop;
    
    // was the item playing?
    var itemWasPlaying = have_class(itemTableE, "playing");
    var nextItemWasPlaying = have_class(nextItemTableE, "playing");

    
    // remove item and next item tables from the HTML DOM
    var parentE = itemTableE.parentNode;
    parentE.removeChild(itemTableE);
    parentE.removeChild(nextItemTableE);
    
    // swap the clip info
    var itemStartPosition = item.itemStartPosition;
    item.itemStartPosition = nextItem.itemStartPosition;
    var itemDuration = item.itemDuration;
    item.itemDuration = nextItem.itemDuration;
    nextItem.itemStartPosition = itemStartPosition;
    nextItem.itemDuration = itemDuration;
    
    // create new item and new prev item tables
    var newItemTableE = create_item_table(item, itemIndex + 1, nextNextItem);
    var newPrevItemTableE = create_item_table(nextItem, itemIndex, item);
    
    // set playing highlighting
    if (itemWasPlaying)
    {
        add_class(newPrevItemTableE, "playing");
    }
    else if (nextItemWasPlaying)
    {
        add_class(newItemTableE, "playing");
    }
    
    // highlight the moved item
    add_class(newItemTableE, "moved-item");
    setTimeout(remove_class, movedItemHighlightTimeout, newItemTableE, "moved-item");
    
    // append or insert the new item and new previous items
    if (nextNextItemTableE == null)
    {
        parentE.appendChild(newPrevItemTableE);
        parentE.appendChild(newItemTableE);
    }
    else
    {
        parentE.insertBefore(newPrevItemTableE, nextNextItemTableE);
        parentE.insertBefore(newItemTableE, nextNextItemTableE);
    }

    
    // scroll the page to the item
    window.scrollBy(newItemTableE.offsetLeft - itemOffsetLeft, newItemTableE.offsetTop - itemOffsetTop);
    
    return true;
}

function move_item_down_handler()
{
    try
    {
        if (changeItemRequest.readyState == 4)
        {
            if (changeItemRequest.status != 200)
            {
                throw "changeItemRequest status not ok";
            }

            var result = eval("(" + changeItemRequest.responseText + ")");
            
            if (itemSourceChangeCount + 1 == result.itemSourceChangeCount)
            {
                if (!html_move_item_down(changeItemRequest.itemId, changeItemRequest.itemIndex))
                {
                    enableItemRequest = true;
                    return;
                }
                
                itemSourceChangeCount = result.itemSourceChangeCount;
            }
            else
            {
                // else we are out of sync and a reload is required
                itemSourceChangeCount = -1;
            }
            
            enableItemRequest = true;
        }
    }
    catch (err)
    {
        enableItemRequest = true;
    }
}

function move_item_down(itemId, itemIndex)
{
    if (enableItemRequest)
    {
        enableItemRequest = false;
        try
        {
            changeItemRequest = new XMLHttpRequest();
            changeItemRequest.itemId = itemId;
            changeItemRequest.itemIndex = itemIndex;
            changeItemRequest.open("GET", "/recorder/session/moveitemdown.json?id=" + itemId + "&index=" + itemIndex, true);
            changeItemRequest.onreadystatechange = move_item_down_handler;
            changeItemRequest.send(null);
        }
        catch (err)
        {
            enableItemRequest = true;
        }
    }
}

function html_move_item_up(itemId, itemIndex)
{
    // get the item table, prev item table and if present, the next item table
    var itemTableE = itemTables[itemId];
    var item = items[itemId];
    if (itemTableE == null || parseInt(itemTableE.getAttribute("item-index")) != itemIndex)
    {
        return false;
    }
    var prevItemTableE = itemTableE.previousSibling;
    if (prevItemTableE == null || parseInt(prevItemTableE.getAttribute("item-id")) >= items.length)
    {
        return false;
    }
    var prevItem = items[parseInt(prevItemTableE.getAttribute("item-id"))];
    var nextItemTableE = itemTableE.nextSibling;
    var nextItem = null;
    if (nextItemTableE != null && parseInt(nextItemTableE.getAttribute("item-id")) < items.length)
    {
        nextItem = items[parseInt(nextItemTableE.getAttribute("item-id"))];
    }

    // store the current page offset for the item so that at the end we move
    // the page so it stays in the same position
    var itemOffsetLeft = itemTableE.offsetLeft;
    var itemOffsetTop = itemTableE.offsetTop;
    
    // was the item playing?
    var itemWasPlaying = have_class(itemTableE, "playing");
    var prevItemWasPlaying = have_class(prevItemTableE, "playing");
    
    
    // remove item and next item tables from the HTML DOM
    var parentE = itemTableE.parentNode;
    parentE.removeChild(itemTableE);
    parentE.removeChild(prevItemTableE);
    
    // swap the clip info
    var itemStartPosition = item.itemStartPosition;
    item.itemStartPosition = prevItem.itemStartPosition;
    var itemDuration = item.itemDuration;
    item.itemDuration = prevItem.itemDuration;
    prevItem.itemStartPosition = itemStartPosition;
    prevItem.itemDuration = itemDuration;
    
    // create new item and next item tables
    var newItemTableE = create_item_table(item, itemIndex - 1, prevItem);
    var newNextItemTableE = create_item_table(prevItem, itemIndex, nextItem);
    
    // set playing highlighting
    if (itemWasPlaying)
    {
        add_class(newNextItemTableE, "playing");
    }
    else if (prevItemWasPlaying)
    {
        add_class(newItemTableE, "playing");
    }
    
    // highlight the moved item
    add_class(newItemTableE, "moved-item");
    setTimeout(remove_class, movedItemHighlightTimeout, newItemTableE, "moved-item");
    
    // append or insert the new item and new next items
    if (nextItemTableE == null)
    {
        parentE.appendChild(newItemTableE);
        parentE.appendChild(newNextItemTableE);
    }
    else
    {
        parentE.insertBefore(newItemTableE, nextItemTableE);
        parentE.insertBefore(newNextItemTableE, nextItemTableE);
    }

    
    // scroll the page to the item
    window.scrollBy(newItemTableE.offsetLeft - itemOffsetLeft, newItemTableE.offsetTop - itemOffsetTop);
    
    return true;
}

function move_item_up_handler()
{
    try
    {
        if (changeItemRequest.readyState == 4)
        {
            if (changeItemRequest.status != 200)
            {
                throw "changeItemRequest status not ok";
            }

            var result = eval("(" + changeItemRequest.responseText + ")");

            if (itemSourceChangeCount + 1 == result.itemSourceChangeCount)
            {
                if (!html_move_item_up(changeItemRequest.itemId, changeItemRequest.itemIndex))
                {
                    enableItemRequest = true;
                    return;
                }
                
                itemSourceChangeCount = result.itemSourceChangeCount;
            }
            else
            {
                // else we are out of sync and a reload is required
                itemSourceChangeCount = -1;
            }
            
            enableItemRequest = true;
        }
    }
    catch (err)
    {
        enableItemRequest = true;
    }
}

function move_item_up(itemId, itemIndex)
{
    if (enableItemRequest)
    {
        enableItemRequest = false;
        try
        {
            changeItemRequest = new XMLHttpRequest();
            changeItemRequest.itemId = itemId;
            changeItemRequest.itemIndex = itemIndex;
            changeItemRequest.open("GET", "/recorder/session/moveitemup.json?id=" + itemId + "&index=" + itemIndex, true);
            changeItemRequest.onreadystatechange = move_item_up_handler;
            changeItemRequest.send(null);
        }
        catch (err)
        {
            enableItemRequest = true;
        }
    }
}

function html_disable_item(itemId, itemIndex)
{
    // move the item down to above the first disabled item or the end
    var newItemIndex;
    for (newItemIndex = itemIndex; newItemIndex < itemTables.length - 1; newItemIndex++)
    {
        var nextItemId = parseInt(itemTables[itemId].nextSibling.getAttribute("item-id"));
        if (items[nextItemId].isDisabled)
        {
            break;
        }
        
        if (!html_move_item_down(itemId, newItemIndex))
        {
            throw "Failed to move html item down";
        }
    }

    // disable the item
    items[itemId].isDisabled = true;
    items[itemId].itemDuration = -1;
    items[itemId].itemStartPosition = -1;

    replace_item_table(itemTables[itemId], items[itemId], newItemIndex, null);
    
    
    // remove 'move item down' button from the previous item by replacing it
    if (newItemIndex > 0)
    {
        var oldPrevItemTableE = itemTables[itemId].previousSibling;
        var prevItemId = parseInt(oldPrevItemTableE.getAttribute("item-id"));
        var prevItemIndex = parseInt(oldPrevItemTableE.getAttribute("item-index"));

        replace_item_table(oldPrevItemTableE, items[prevItemId], prevItemIndex, items[itemId]);
    }

    return true;
}

function disable_item_handler()
{
    try
    {
        if (changeItemRequest.readyState == 4)
        {
            if (changeItemRequest.status != 200)
            {
                throw "changeItemRequest status not ok";
            }

            var result = eval("(" + changeItemRequest.responseText + ")");

            if (itemSourceChangeCount + 1 == result.itemSourceChangeCount)
            {
                if (!html_disable_item(changeItemRequest.itemId, changeItemRequest.itemIndex))
                {
                    enableItemRequest = true;
                    return;
                }
                
                itemSourceChangeCount = result.itemSourceChangeCount;
            }
            else
            {
                // else we are out of sync and a reload is required
                itemSourceChangeCount = -1;
            }
            
            enableItemRequest = true;
        }
    }
    catch (err)
    {
        enableItemRequest = true;
    }
}

function disable_item(itemId, itemIndex)
{
    if (enableItemRequest)
    {
        enableItemRequest = false;
        try
        {
            changeItemRequest = new XMLHttpRequest();
            changeItemRequest.itemId = itemId;
            changeItemRequest.itemIndex = itemIndex;
            changeItemRequest.open("GET", "/recorder/session/disableitem.json?id=" + itemId + "&index=" + itemIndex, true);
            changeItemRequest.onreadystatechange = disable_item_handler;
            changeItemRequest.send(null);
        }
        catch (err)
        {
            enableItemRequest = true;
        }
    }
}

function html_enable_item(itemId, itemIndex)
{
    // move item up above the disabled items
    var newItemIndex;
    for (newItemIndex = itemIndex; newItemIndex >= 1; newItemIndex--)
    {
        var prevItemId = parseInt(itemTables[itemId].previousSibling.getAttribute("item-id"));
        if (!items[prevItemId].isDisabled && !items[prevItemId].isJunk)
        {
            break;
        }
        
        if (!html_move_item_up(itemId, newItemIndex))
        {
            throw "Failed to move html item up";
        }
    }

    // enable the item
    items[itemId].isDisabled = false;

    replace_item_table(itemTables[itemId], items[itemId], newItemIndex, null);

    
    // add 'move item down' button to the previous item by replacing it
    if (newItemIndex > 0)
    {
        var oldPrevItemTableE = itemTables[itemId].previousSibling;
        var prevItemId = parseInt(oldPrevItemTableE.getAttribute("item-id"));
        var prevItemIndex = parseInt(oldPrevItemTableE.getAttribute("item-index"));

        replace_item_table(oldPrevItemTableE, items[prevItemId], prevItemIndex, items[itemId]);
    }
    
    return true;
}

function enable_item_handler()
{
    try
    {
        if (changeItemRequest.readyState == 4)
        {
            if (changeItemRequest.status != 200)
            {
                throw "changeItemRequest status not ok";
            }

            var result = eval("(" + changeItemRequest.responseText + ")");

            if (itemSourceChangeCount + 1 == result.itemSourceChangeCount)
            {
                if (!html_enable_item(changeItemRequest.itemId, changeItemRequest.itemIndex))
                {
                    enableItemRequest = true;
                    return;
                }
                
                itemSourceChangeCount = result.itemSourceChangeCount;
            }
            else
            {
                // else we are out of sync and a reload is required
                itemSourceChangeCount = -1;
            }
            
            enableItemRequest = true;
        }
    }
    catch (err)
    {
        enableItemRequest = true;
    }
}

function enable_item(itemId, itemIndex)
{
    if (enableItemRequest)
    {
        enableItemRequest = false;
        try
        {
            changeItemRequest = new XMLHttpRequest();
            changeItemRequest.itemId = itemId;
            changeItemRequest.itemIndex = itemIndex;
            changeItemRequest.open("GET", "/recorder/session/enableitem.json?id=" + itemId + "&index=" + itemIndex, true);
            changeItemRequest.onreadystatechange = enable_item_handler;
            changeItemRequest.send(null);
        }
        catch (err)
        {
            enableItemRequest = true;
        }
    }
}

function play_item(itemId, itemIndex)
{
    var itemPlayRequest = new XMLHttpRequest();
    itemPlayRequest.open("GET", "/recorder/session/playitem?id=" + itemId + "&index=" + itemIndex, true);
    itemPlayRequest.send(null);
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
            
            enableItemRequest = true;
        }
    } 
    catch (err)
    {
        enableItemRequest = true;
        // ignore failure
    }
}

function mark_item_start()
{
    if (enableItemRequest)
    {
        enableItemRequest = false;
        try
        {
            itemMarkRequest = new XMLHttpRequest();
            itemMarkRequest.open("GET", "/recorder/session/markitemstart.json", true);
            itemMarkRequest.onreadystatechange = mark_item_start_handler;
            itemMarkRequest.send(null);
        }
        catch (err)
        {
            enableItemRequest = true;
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
            
            enableItemRequest = true;
        }
    } 
    catch (err)
    {
        enableItemRequest = true;
        // ignore failure
    }
}

function clear_item()
{
    if (enableItemRequest)
    {
        enableItemRequest = false;
        try
        {
            // cannot clear the first item or when not positioned at the start of an item
            if (playingItemIndex <= 0 || playingItemPosition != 0)
            {
                enableItemRequest = true;
                return;
            }
        
            itemMarkRequest = new XMLHttpRequest();
            itemMarkRequest.open("GET", "/recorder/session/clearitem.json?id=" + playingItemId + "&index=" + playingItemIndex, true);
            itemMarkRequest.onreadystatechange = clear_item_handler;
            itemMarkRequest.send(null);
        }
        catch (err)
        {
            enableItemRequest = true;
        }
    }
}

function init_items()
{
    set_source_info(null, -1);
    
    // override replay command to set and remove marks
    set_user_mark = mark_item_start;
    remove_user_mark = clear_item;
    
    request_status();
}


