function (event, funcs) {
    function legend(value) {
        var el = event.icon.find('.mp-range');
        if (value > 0.5) {
            el.addClass('mp-doubled');
        } else {
            el.removeClass('mp-doubled');
        }
    }
    if (event.type == 'start') {
        var ports = event.ports || [];
        for (var i = 0; i < ports.length; i++) {
            if (ports[i].symbol == 'time_mod') {
                legend(ports[i].value);
            }
        }
        // range lever: clicking a legend line selects that range directly
        // (the stock film widget would only step through them in order)
        var hits = event.icon.find('.mp-range-hit');
        hits.on('mousedown touchstart', function (e) {
            e.stopPropagation();
        });
        hits.on('click', function (e) {
            e.preventDefault();
            e.stopPropagation();
            funcs.set_port_value('range', parseFloat(this.getAttribute('data-value')));
        });
    } else if (event.type == 'change' && event.symbol == 'time_mod') {
        legend(event.value);
    }
}
