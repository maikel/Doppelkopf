import React, { Component } from 'react';
import { 
  Container, Row, Col, 
  InputGroup, Input, Button, InputGroupAddon, ButtonGroup
} from 'reactstrap';
import './Lobby.css';
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome';

function countActivePlayers(players) {
  let active = players.filter(function (p) {
    return p !== null;
  });
  return active.length;
}

class Lobby extends Component {
  constructor(props) {
    super(props);
    this.send = props.send;
    this.state = {
      tableNameValue: "",
      selectedItem: null,
    };
    this.createTable = this.createTable.bind(this);
    this.deleteTable = this.deleteTable.bind(this);
    this.joinTable = this.joinTable.bind(this);
    this.onTableNameChange = this.onTableNameChange.bind(this);
    this.toggleItem = this.toggleItem.bind(this);
  }

  createTable(event) {
    if (this.state.tableNameValue !== "") {
      this.send({command: "create_table", name: this.state.tableNameValue});
    }
  }

  deleteTable(name) {
    this.send({command: "destroy_table", name: name});
  }

  joinTable(name) {
    this.send({command: "join_table", name: name});
  }

  onTableNameChange(event) {
    this.setState({tableNameValue: event.target.value});
  }

  toggleItem(event) {
    let key = parseInt(event.currentTarget.dataset.id);
    if (this.state.selectedItem === key) {
      this.setState({selectedItem: null});  
    } else {
      this.setState({selectedItem: key});
    }
  }

  render() {
    const list = this.props.tables.map((entry, index) => {
      return (
      <Row key={index} className="tableListEntry">
        <Col>{entry.name}</Col>
        <Col>{countActivePlayers(entry.players)} / 4</Col>
        <Col>{entry.observers.length}</Col>
        <Col xs="auto"><a onClick={(e) => { this.joinTable(entry.name); }} href="#" className="signIn"><FontAwesomeIcon icon="sign-in-alt"/></a></Col>
        <Col xs="auto"><a onClick={(e) => { this.deleteTable(entry.name); }}  href="#" className="deleteTable"><FontAwesomeIcon icon="trash-alt" /></a></Col>
      </Row>);
    });
    return (
      <div className="Lobby">
        <Container className="tableList">
          { list }
        </Container>
        <InputGroup>
          <InputGroupAddon addonType="prepend"><Button onClick={this.createTable}>Tisch Erstellen</Button></InputGroupAddon>
          <Input onChange={this.onTableNameChange} />
        </InputGroup>
      </div>
    );
  }
}

export default Lobby;