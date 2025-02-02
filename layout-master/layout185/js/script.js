/**
 * scrolldir - Vertical scroll direction in CSS
 * @version v1.2.23
 * @link https://github.com/dollarshaveclub/scrolldir.git
 * @author Patrick Fisher <patrick@pwfisher.com>
 * @license MIT
**/
!function(t,e){"object"==typeof exports&&"undefined"!=typeof module?module.exports=e():"function"==typeof define&&define.amd?define(e):t.scrollDir=e()}(this,function(){"use strict";function t(){var t=r.scrollY||r.pageYOffset,e=c.timeStamp,p="down"===i?Math.max:Math.min,v=f.offsetHeight-r.innerHeight;if(t=Math.max(0,t),t=Math.min(v,t),s.unshift({y:t,t:e}),s.pop(),t===p(l,t))return m=e,void(l=t);var h=e-u;if(h>m){l=t;for(var b=0;b<d&&(s[b]&&!(s[b].t<h));b+=1)l=p(l,s[b].y)}Math.abs(t-l)>a&&(l=t,m=e,i="down"===i?"up":"down",o.setAttribute(n,i))}function e(e){return c=e,r.requestAnimationFrame(t)}var n="data-scrolldir",i="down",o=document.documentElement,r=window,f=document.body,d=32,u=512,a=64,s=Array(d),c=void 0,l=void 0,m=0;return function(t){return t&&(t.attribute&&(n=t.attribute),t.el&&(o=t.el),t.win&&(r=t.win),t.dir&&(i=t.dir),!0===t.off)?(o.setAttribute(n,"off"),r.removeEventListener("scroll",e)):(l=r.scrollY||r.pageYOffset,o.setAttribute(n,i),r.addEventListener("scroll",e))}});

scrollDir();