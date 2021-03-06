(function ($) {
  Drupal.behaviors = Drupal.behaviors || {};
  Drupal.behaviors.datatable_search_results = {
    attach: function (context, settings) {
      $(document).ready(function () {
        function ucwords(value) {
          return (value + '')
              .replace(/^(.)|\s+(.)/g, function (value) {
                return value.toUpperCase();
              });
        }

        $('#tripal-elastic-search-results-table tr td, .es-squish').each(function () {
          var text = $(this).html();
          var array = text.split('<br>');

          if (array.length > 2) {
            var div = $('<div />', {});
            div.append(array.shift() + '<br>' + array.shift());
            var hidden = $('<div />', {'class': 'hidden-hit'}).css('display', 'none');
            hidden.append(array.join('<br>'));
            div.append(hidden);
            div.append('<br>');
            var btn = $('<button />', {
              'type': 'button',
              'class': 'btn btn-secondary btn-sm'
            }).html('Show More')
                .click(function (e) {
                  e.preventDefault();

                  var hidden_hit = hidden;
                  if (hidden_hit.hasClass('is_open')) {
                    hidden_hit.removeClass('is_open');
                    hidden_hit.slideUp();
                    btn.html('Show More');
                  }
                  else {
                    hidden_hit.slideDown();
                    hidden_hit.addClass('is_open');
                    btn.html('Show Less');
                  }
                });
            div.append(btn);
            $(this).html(div);
          }
        });

        var $table = $('#tripal-elastic-search-results-table');
        if ($table) {
          $table.find('th').each(function () {
            $(this).css({
              'white-space': 'nowrap'
            });
            var a = $(this).find('a').first();
            var img = a.find('img').css('margin-left', '10px');
            var text = $(this).data('elasticTitle');
            a.html(ucwords(text.replace('_', ' '))).append(img);
          });
        }
      });
    }
  };
})(jQuery);