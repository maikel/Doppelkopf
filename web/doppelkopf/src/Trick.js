import React from 'react';
import { Card } from './Hand';
import './Trick.css';

function EmptySlot(props) {
  return (<div className="EmptySlot" />);
}

function shiftAndWrapAround(array, shift) {
  for (var i = 0; i < shift; ++i) {
    array.push(array.shift());
  }
  return array;
}

export default function Trick(props) {
  const slots = shiftAndWrapAround(props.cards.map((card, index) => {
    var jsxobj = (<EmptySlot />);
    if (card !== null && Object.keys(card).length !== 0) {
      jsxobj = (<Card color={card.color} face={card.face} />);
    }
    return (<div id={'Slot-' + index}>{jsxobj}</div>);
  }), 2);
  return (
    <div className="Trick">
      { slots }
    </div>
  );
}