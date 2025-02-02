CSSPlugin.useSVGTransformAttr = false;
tmax_options = 
  delay: 0,
  repeat: 0
tmax_tl = new TimelineMax(tmax_options)
polyonlion_shapes = $('svg > g polygon')
polyonlion_stagger = 0.025
polyonlion_duration = 0.1675
polyonlion_staggerFrom = 
  scale: 0
  opacity: 0
  transformOrigin: 'center center'
polyonlion_staggerTo = 
  opacity: 1
  scale: 1
  ease: Linear.easeOut
tmax_tl.staggerFromTo polyonlion_shapes, polyonlion_duration, polyonlion_staggerFrom, polyonlion_staggerTo, polyonlion_stagger, 0