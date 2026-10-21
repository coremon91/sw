"use strict";
(()=>{const selected=new URLSearchParams(location.search).get("studioLayer");if(!selected)return;
  for(const [id,element] of Object.entries({programHead:"program-head",lowerThird:"lower-third",naturalDisaster:"natural-disaster"})){
    if(id!==selected)document.getElementById(element).style.display="none";
  }
})();
