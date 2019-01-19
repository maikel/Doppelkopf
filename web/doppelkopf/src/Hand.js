import React from 'react';
import './Hand.css';

function PathToSvg(color, face) {
  const colorFirstLetter = color.charAt(0).toUpperCase();
  const faceFirstLetter = face.charAt(0).toUpperCase();
  if (faceFirstLetter === 'T') {
    return "./images/10" + colorFirstLetter + ".svg";
  } else if (faceFirstLetter === 'N') {
    return "./images/9" + colorFirstLetter + ".svg";
  }
  return "./images/" + faceFirstLetter + colorFirstLetter + ".svg";
}

export function Card(props) {
  const svg = require(`${PathToSvg(props.color, props.face)}`);
  return (<img src={svg} className="Card" alt="A card" />);
}

export default function Hand(props) {
  const cards = props.cards.map((card) => 
    <Card color={card.color} face={card.face} />
  );
  return (
    <div className="Hand">
      {cards}
    </div>
  );
}