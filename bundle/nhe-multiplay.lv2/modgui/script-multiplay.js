function (event, funcs) {
    var icon = event.icon;

    // range legend: original or doubled times, following Time Mod
    function legend(value) {
        var el = icon.find('.mp-range');
        if (value > 0.5) {
            el.addClass('mp-doubled');
        } else {
            el.removeClass('mp-doubled');
        }
    }

    // show a footswitch "down" for a moment when its control changes
    // from outside the face (a hardware footswitch, MIDI, a preset)
    function press(selector) {
        var el = icon.find(selector);
        el.addClass('mp-press');
        setTimeout(function () { el.removeClass('mp-press'); }, 150);
    }

    if (event.type == 'start') {
        var ports = event.ports || [];
        for (var i = 0; i < ports.length; i++) {
            if (ports[i].symbol == 'time_mod') {
                legend(ports[i].value);
            }
        }
        // range lever: clicking a legend line selects that range directly
        var hits = icon.find('.mp-range-hit');
        hits.on('mousedown touchstart', function (e) {
            e.stopPropagation();
        });
        hits.on('click', function (e) {
            e.preventDefault();
            e.stopPropagation();
            funcs.set_port_value('range', parseFloat(this.getAttribute('data-value')));
        });
    } else if (event.type == 'change') {
        if (event.symbol == 'time_mod') {
            legend(event.value);   // the button itself latches down via its "on" class
        } else if (event.symbol == 'hold') {
            press('.mp-fs-hold');
        } else if (event.symbol == ':bypass') {
            press('.mp-fs-effect');
        } else if (event.symbol == 'slam') {
            // momentary: the footswitch stays down for as long as Slam is held
            var el = icon.find('.mp-fs-slam');
            if (event.value > 0.5) {
                el.addClass('mp-press');
            } else {
                el.removeClass('mp-press');
            }
        }
    }
}
