// Generated by CoffeeScript 1.12.7
(function() {
  var Easing, Pen, Point, Trig,
    bind = function(fn, me){ return function(){ return fn.apply(me, arguments); }; };

  Easing = (function() {
    function Easing() {}

    Easing.curve = function(t, b, c, d) {
      if ((t /= d / 2) < 1) {
        return c / 2 * Math.pow(t, 2) + b;
      } else {
        return -c / 2 * ((t - 1) * (t - 3) - 1) + b;
      }
    };

    Easing.linear = function(t, b, c, d) {
      return c * t / d + b;
    };

    return Easing;

  })();

  Trig = (function() {
    function Trig() {}

    Trig.rad = function(deg) {
      return deg * Math.PI / 180;
    };

    Trig.deg = function(rad) {
      return rad * 180 / Math.PI;
    };

    Trig.getBaseAngleFromPoints = function(dx, dy) {
      var angle;
      angle = Math.atan(dy / dx);
      return Math.abs(angle);
    };

    Trig.getQuadrant = function(dx, dy) {
      if (dy >= 0) {
        if (dx >= 0) {
          return 1;
        } else {
          return 2;
        }
      } else {
        if (dx < 0) {
          return 3;
        } else {
          return 4;
        }
      }
    };

    Trig.getAngleFromPoints = function(p1, p2) {
      var baseAngle, dx, dy;
      dx = p2.x - p1.x;
      dy = p2.y - p1.y;
      baseAngle = this.getBaseAngleFromPoints(dx, dy);
      switch (this.getQuadrant(dx, dy)) {
        case 1:
          return baseAngle;
        case 2:
          return Math.PI - baseAngle;
        case 3:
          return Math.PI + baseAngle;
        case 4:
          return 2 * Math.PI - baseAngle;
      }
    };

    Trig.getDistanceBetweenPoints = function(p1, p2) {
      var distance, dx, dy;
      dx = p2.x - p1.x;
      dy = p2.y - p1.y;
      return distance = Math.sqrt(dx * dx + dy * dy);
    };

    Trig.getPointFromAngle = function(origin, angle, distance) {
      var x, y;
      x = origin.x, y = origin.y;
      if (angle === Math.PI) {
        return {
          x: x - distance,
          y: y
        };
      } else if (angle === Math.PI / 2) {
        return {
          x: x,
          y: y + distance
        };
      } else if (angle === Math.PI * 1.5) {
        return {
          x: x,
          y: y - distance
        };
      } else if (angle === 0) {
        return {
          x: x + distance,
          y: y
        };
      } else {
        return {
          x: Math.cos(angle) * distance + x,
          y: Math.sin(angle) * distance + y
        };
      }
    };

    return Trig;

  })();

  Point = (function() {
    function Point(ctx, x1, y1, size) {
      this.ctx = ctx;
      this.x = x1;
      this.y = y1;
      this.size = size != null ? size : 20;
      this.update = bind(this.update, this);
      this.draw();
    }

    Point.prototype.draw = function(x, y) {
      if (x == null) {
        x = this.x;
      }
      if (y == null) {
        y = this.y;
      }
      this.ctx.moveTo(x, y);
      this.ctx.beginPath();
      this.ctx.arc(x, y, this.size / 2, 0, Math.PI * 2);
      this.ctx.fillStyle = 'white';
      return this.ctx.fill();
    };

    Point.prototype.update = function(mouse) {
      var angle, dist, offset, point, range;
      range = 125;
      dist = Trig.getDistanceBetweenPoints(mouse, this);
      if (dist > range) {
        return this.draw() || true;
      }
      angle = Trig.getAngleFromPoints(mouse, this);
      offset = Easing.curve(dist, range, -range, range);
      point = Trig.getPointFromAngle(this, angle, offset);
      return this.draw(point.x, point.y);
    };

    return Point;

  })();

  Pen = (function() {
    function Pen() {
      this.handleMouse = bind(this.handleMouse, this);
      this.$canvas = $('canvas');
      this.ctx = this.$canvas.get(0).getContext('2d');
      this.$canvas.attr({
        height: this.$canvas.height(),
        width: this.$canvas.width()
      });
      this.$canvas.css({
        height: this.$canvas.height(),
        width: this.$canvas.width()
      });
      this.points = this.getPoints();
      $(document).on('mousemove', this.handleMouse);
    }

    Pen.prototype.getPoints = function() {
      var angle, dist, i, j, origin, point, results;
      origin = {
        x: this.$canvas.width() / 2,
        y: this.$canvas.height() / 2
      };
      results = [];
      for (i = j = 1; j <= 65; i = ++j) {
        angle = Math.PI * 0.2 * i;
        dist = i * 4;
        point = Trig.getPointFromAngle(origin, angle, dist);
        results.push(new Point(this.ctx, point.x, point.y, i));
      }
      return results;
    };

    Pen.prototype.handleMouse = function(event) {
      var j, len, mouse, point, ref, results;
      mouse = {
        x: event.pageX,
        y: event.pageY
      };
      this.ctx.clearRect(0, 0, this.$canvas.width(), this.$canvas.height());
      ref = this.points || [];
      results = [];
      for (j = 0, len = ref.length; j < len; j++) {
        point = ref[j];
        results.push(point.update(mouse));
      }
      return results;
    };

    return Pen;

  })();

  new Pen;

}).call(this);
