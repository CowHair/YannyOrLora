(function($) {

	"use strict";

	var fullHeight = function() {

		$('.js-fullheight').css('height', $(window).height());
		$(window).resize(function(){
			$('.js-fullheight').css('height', $(window).height());
		});

	};
	fullHeight();

	var burgerMenu = function() {

		$('.js-colorlib-nav-toggle').on('click', function(event) {
			event.preventDefault();
			var $this = $(this);
			if( $('body').hasClass('menu-show') ) {
				$('body').removeClass('menu-show');
				$('#colorlib-main-nav > .js-colorlib-nav-toggle').removeClass('show');
			} else {
				$('body').addClass('menu-show');
				setTimeout(function(){
					$('#colorlib-main-nav > .js-colorlib-nav-toggle').addClass('show');
				}, 900);
			}
		})
	};
	burgerMenu();


})(jQuery);

// Menu color
$(document).ready(function() {
  $('#colorlib-main-nav ul li a span').on('mouseenter', function() {
    $(this).css('color', '#f9bf3b');
  }).on('mouseleave', function() {
    // only reset to white if it's NOT the active page
    if (!$(this).closest('li').hasClass('active')) {
      $(this).css('color', '#fff');
    }
  });
});