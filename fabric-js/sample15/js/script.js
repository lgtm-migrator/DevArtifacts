(function() {
  var canvas = this.__canvas = new fabric.Canvas('c');
  fabric.Object.prototype.transparentCorners = false;

  var radius = 300;

  fabric.Image.fromURL('./images/pug_small.jpg', function(img) {
    img.scale(0.5).set({
      left: 100,
      top: 100,
      angle: -15,
      clipTo: function (ctx) {
        ctx.arc(0, 0, radius, 0, Math.PI * 2, true);
      }
    });
    canvas.add(img).setActiveObject(img);
  });

  (function animate() {
    fabric.util.animate({
      startValue: Math.round(radius) === 50 ? 50 : 300,
      endValue: Math.round(radius) === 50 ? 300 : 50,
      duration: 1000,
      onChange: function(value) {
        radius = value;
        canvas.renderAll();
      },
      onComplete: animate
    });
  })();
})();
