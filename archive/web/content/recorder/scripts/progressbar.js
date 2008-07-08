var pbCenterLeft = 28;
var pbCenterRight = 772;
var pbPointerWidth = 7;
var pbPointerCentre = 3;
var pbMarkWidth = 3;
var pbMarkCentre = 1;
var pgItemClipChangeCount = -1;
var pgMarks = new Array();
var pgMarksDuration = -1;

var pgMarkImage = new Image();
pgMarkImage.src = "images/progress_mark.png";
var pgJunkMarkImage = new Image();
pgJunkMarkImage.src = "images/junk_progress_mark.png";


function handle_progressbar_click(ele, event)
{
    var posX = event.clientX + document.body.scrollLeft + document.documentElement.scrollLeft - ele.parentNode.offsetLeft;
    
    if (posX < pbCenterLeft)
    {
        seek_home();
    }
    else if (posX > pbCenterRight)
    {
        seek_end();
    }
    else
    {
        seek_percentage(100 * (posX - pbCenterLeft) / (pbCenterRight - pbCenterLeft));
    }
}

function update_progressbar_pointer(position, duration)
{
    var pointerEle = document.getElementById("progress-bar-pointer");
    
    if (position < 0 || duration <= 0)
    {
        update_tag_attr("progress-bar-pointer", "style", "display: none");
        return;
    }
 
    var pagePos;
    if (duration <= 1)
    {
        pagePos = (pbCenterLeft - pbMarkCentre);
    }
    else
    {
        pagePos = (pbCenterLeft - pbPointerCentre) + (pbCenterRight - pbCenterLeft) * (position / (duration - 1));
    }
    update_tag_attr("progress-bar-pointer", "style", "left:" + pagePos + "px");
}

function add_progressbar_mark(position, duration, isJunk)
{
    var pagePos;
    if (duration <= 1)
    {
        pagePos = (pbCenterLeft - pbMarkCentre);
    }
    else
    {
        pagePos = (pbCenterLeft - pbMarkCentre) + (pbCenterRight - pbCenterLeft) * (position / (duration - 1));
    }
    
    var markE = document.createElement("img");
    markE.setAttribute("class", "progress-bar-mark");
    markE.setAttribute("src", isJunk ? pgJunkMarkImage.src : pgMarkImage.src);
    markE.setAttribute("style", "left:" + pagePos + "px");
    markE.setAttribute("position", position);
    
    var progressBarE = document.getElementById("progress-bar");
    progressBarE.appendChild(markE);

    pgMarks.push(markE);
}

function remove_progressbar_mark(position)
{
    var i;
    for (i = 0; i < pgMarks.length; i++)
    {
        if (pgMarks[i].getAttribute("position") == position)
        {
            pgMarks[i].parentNode.removeChild(pgMarks[i]);
            if (i < pgMarks.length - 1)
            {
                // last mark is no longer a junk mark
                pgMarks[pgMarks.length - 1].setAttribute("src", pgMarkImage.src);
            }
            pgMarks.splice(i, 1);
            return true;
        }
    }
    
    return false;
}

function init_progressbar_marks(itemClipInfo, duration)
{
    var i;
    for (i = 0; i < pgMarks.length; i++)
    {
        pgMarks[i].parentNode.removeChild(pgMarks[i]);
    }
    pgMarks.splice(0, pgMarks.length);
    
    if (itemClipInfo == null)
    {
        pgItemClipChangeCount = -1;
        pgMarksDuration = -1;
        return;
    }
    
    var i;
    for (i = 0; i < itemClipInfo.items.length; i++)
    {
        // note: itemStartPosition == 0 if the item is the first item or the item
        // has been chunked. This means that the item start mark dissappears when the item
        // has been chunked
        if (itemClipInfo.items[i].itemStartPosition > 0)
        {
            add_progressbar_mark(itemClipInfo.items[i].itemStartPosition, duration, itemClipInfo.items[i].isJunk);
        }
    }
    
    pgItemClipChangeCount = itemClipInfo.itemClipChangeCount;
    pgMarksDuration = duration;
}

function set_progress_bar_state(state, chunkingPosition, chunkingDuration)
{
    var progressBarE = document.getElementById("progress-bar-input");
    var chunkPercentageE = document.getElementById("chunk-percentage");
    
    // progress bar position
    switch (state)
    {
        case 4: // PREPARE_CHUNKING_SESSION_STATE
            progressBarE.setAttribute("style", "background-position: -" + pbCenterRight + "px 0px");
            break;

        case 5: // CHUNKING_SESSION_STATE
            var relPos = (chunkingDuration <= 0) ? 0 : chunkingPosition / chunkingDuration;
            var xPos = -pbCenterRight + relPos * (pbCenterRight - pbCenterLeft);
            
            progressBarE.setAttribute("style", "background-position: " + xPos + "px 0px");
            break;

        default:
            progressBarE.setAttribute("style", "background-position: -" + pbCenterLeft + "px 0px");
            break;
    }
    
    // chunking percentage
    switch (state)
    {
        case 5:
            remove_class(chunkPercentageE, "hide-node");

            var relPos = (chunkingDuration <= 0) ? 0 : chunkingPosition / chunkingDuration;
            chunkPercentageE.innerHTML = Math.floor(100 * relPos) + "%";
            break;
          
        default:
            add_class(chunkPercentageE, "hide-node");
            break;
    }
    
}


